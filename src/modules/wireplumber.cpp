#include "modules/wireplumber.hpp"

waybar::modules::Wireplumber::Wireplumber(const std::string& id, const Json::Value& config)
    : ALabel(config, "wireplumber", id, "{format_sink} {format_source}"),
      wp_core_(nullptr),
      apis_(nullptr),
      om_(nullptr),
      pending_plugins_(0),
      sinkmuted_(false),
      sinkvolume_(0.0),
      sinknode_id_(0),
      sourcemuted_(false),
      sourcevolume_(0.0),
      sourcenode_id_(0) {
  wp_init(WP_INIT_ALL);
  wp_core_ = wp_core_new(NULL, NULL);
  apis_ = g_ptr_array_new_with_free_func(g_object_unref);
  om_ = wp_object_manager_new();

  prepare();

  loadRequiredApiModules();

  if (!wp_core_connect(wp_core_)) {
    throw std::runtime_error("Could not connect to PipeWire\n");
  }

  g_signal_connect_swapped(om_, "installed", (GCallback)onObjectManagerInstalled, this);

  activatePlugins();

  dp.emit();
}

waybar::modules::Wireplumber::~Wireplumber() {
  g_clear_pointer(&apis_, g_ptr_array_unref);
  g_clear_object(&om_);
  g_clear_object(&wp_core_);
}

uint32_t waybar::modules::Wireplumber::getDefaultSinkNodeId(waybar::modules::Wireplumber* self) {
  uint32_t id;
  g_autoptr(WpPlugin) def_nodes_api = wp_plugin_find(self->wp_core_, "default-nodes-api");

  if (!def_nodes_api) {
    throw std::runtime_error("Default nodes API is not loaded\n");
  }

  g_signal_emit_by_name(def_nodes_api, "get-default-node", "Audio/Sink", &id);

  if (id <= 0 || id >= G_MAXUINT32) {
    auto err = fmt::format("'{}' is not a valid ID (returned by default-nodes-api)\n", id);
    throw std::runtime_error(err);
  }

  return id;
}

uint32_t waybar::modules::Wireplumber::getDefaultSourceNodeId(waybar::modules::Wireplumber* self) {
  uint32_t id;
  g_autoptr(WpPlugin) def_nodes_api = wp_plugin_find(self->wp_core_, "default-nodes-api");

  if (!def_nodes_api) {
    throw std::runtime_error("Default nodes API is not loaded\n");
  }

  g_signal_emit_by_name(def_nodes_api, "get-default-node", "Audio/Source", &id);

  if (id <= 0 || id >= G_MAXUINT32) {
    auto err = fmt::format("'{}' is not a valid ID (returned by default-nodes-api)\n", id);
    throw std::runtime_error(err);
  }

  return id;
}

void waybar::modules::Wireplumber::updateSinkNodeName(waybar::modules::Wireplumber* self) {
  auto proxy = static_cast<WpProxy*>(
      wp_object_manager_lookup(self->om_, WP_TYPE_GLOBAL_PROXY, WP_CONSTRAINT_TYPE_G_PROPERTY,
                               "bound-id", "=u", self->sinknode_id_, NULL));

  if (!proxy) {
    throw std::runtime_error(fmt::format("Object '{}' not found\n", self->sinknode_id_));
  }

  g_autoptr(WpProperties) properties =
      WP_IS_PIPEWIRE_OBJECT(proxy) ? wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(proxy))
                                   : wp_properties_new_empty();
  g_autoptr(WpProperties) global_p = wp_global_proxy_get_global_properties(WP_GLOBAL_PROXY(proxy));
  properties = wp_properties_ensure_unique_owner(properties);
  wp_properties_add(properties, global_p);
  wp_properties_set(properties, "object.id", NULL);
  auto nick = wp_properties_get(properties, "node.nick");
  auto description = wp_properties_get(properties, "node.description");

  self->sinknode_name_ = nick ? nick : description;
}

void waybar::modules::Wireplumber::updateSourceNodeName(waybar::modules::Wireplumber* self) {
  auto proxy = static_cast<WpProxy*>(
      wp_object_manager_lookup(self->om_, WP_TYPE_GLOBAL_PROXY, WP_CONSTRAINT_TYPE_G_PROPERTY,
                               "bound-id", "=u", self->sourcenode_id_, NULL));

  if (!proxy) {
    throw std::runtime_error(fmt::format("Object '{}' not found\n", self->sourcenode_id_));
  }

  g_autoptr(WpProperties) properties =
      WP_IS_PIPEWIRE_OBJECT(proxy) ? wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(proxy))
                                   : wp_properties_new_empty();
  g_autoptr(WpProperties) global_p = wp_global_proxy_get_global_properties(WP_GLOBAL_PROXY(proxy));
  properties = wp_properties_ensure_unique_owner(properties);
  wp_properties_add(properties, global_p);
  wp_properties_set(properties, "object.id", NULL);
  auto nick = wp_properties_get(properties, "node.nick");
  auto description = wp_properties_get(properties, "node.description");

  self->sourcenode_name_ = nick ? nick : description;
}

void waybar::modules::Wireplumber::updateSinkVolume(waybar::modules::Wireplumber* self) {
  double vol;
  GVariant* variant = NULL;
  g_autoptr(WpPlugin) mixer_api = wp_plugin_find(self->wp_core_, "mixer-api");
  g_signal_emit_by_name(mixer_api, "get-volume", self->sinknode_id_, &variant);
  if (!variant) {
    auto err = fmt::format("Sink Node {} does not support volume\n", self->sinknode_id_);
    throw std::runtime_error(err);
  }

  g_variant_lookup(variant, "volume", "d", &vol);
  g_variant_lookup(variant, "mute", "b", &self->sinkmuted_);
  g_clear_pointer(&variant, g_variant_unref);

  self->sinkvolume_ = std::round(vol * 100.0F);
  self->dp.emit();
}

void waybar::modules::Wireplumber::updateSourceVolume(waybar::modules::Wireplumber* self) {
  double vol;
  GVariant* variant = NULL;
  g_autoptr(WpPlugin) mixer_api = wp_plugin_find(self->wp_core_, "mixer-api");
  g_signal_emit_by_name(mixer_api, "get-volume", self->sourcenode_id_, &variant);
  if (!variant) {
    auto err = fmt::format("Source Node {} does not support volume\n", self->sourcenode_id_);
    throw std::runtime_error(err);
  }

  g_variant_lookup(variant, "volume", "d", &vol);
  g_variant_lookup(variant, "mute", "b", &self->sourcemuted_);
  g_clear_pointer(&variant, g_variant_unref);

  self->sourcevolume_ = std::round(vol * 100.0F);
  self->dp.emit();
}

void waybar::modules::Wireplumber::onObjectManagerInstalled(waybar::modules::Wireplumber* self) {
  self->sinknode_id_ = self->config_["sinknode-id"].isInt() ? self->config_["sinknode-id"].asInt()
                                                            : getDefaultSinkNodeId(self);
  self->sourcenode_id_ = self->config_["sourcenode-id"].isInt()
                             ? self->config_["sourcenode-id"].asInt()
                             : getDefaultSourceNodeId(self);

  g_autoptr(WpPlugin) mixer_api = wp_plugin_find(self->wp_core_, "mixer-api");

  updateSinkVolume(self);
  updateSourceVolume(self);
  updateSinkNodeName(self);
  updateSourceNodeName(self);
  g_signal_connect_swapped(mixer_api, "changed", (GCallback)updateSinkVolume, self);
  g_signal_connect_swapped(mixer_api, "changed", (GCallback)updateSourceVolume, self);
}

void waybar::modules::Wireplumber::onPluginActivated(WpObject* p, GAsyncResult* res,
                                                     waybar::modules::Wireplumber* self) {
  g_autoptr(GError) error = NULL;

  if (!wp_object_activate_finish(p, res, &error)) {
    throw std::runtime_error(error->message);
  }

  if (--self->pending_plugins_ == 0) {
    wp_core_install_object_manager(self->wp_core_, self->om_);
  }
}

void waybar::modules::Wireplumber::activatePlugins() {
  for (uint16_t i = 0; i < apis_->len; i++) {
    WpPlugin* plugin = static_cast<WpPlugin*>(g_ptr_array_index(apis_, i));
    pending_plugins_++;
    wp_object_activate(WP_OBJECT(plugin), WP_PLUGIN_FEATURE_ENABLED, NULL,
                       (GAsyncReadyCallback)onPluginActivated, this);
  }
}

void waybar::modules::Wireplumber::prepare() {
  wp_object_manager_add_interest(om_, WP_TYPE_NODE, NULL);
  wp_object_manager_add_interest(om_, WP_TYPE_GLOBAL_PROXY, NULL);
  wp_object_manager_request_object_features(om_, WP_TYPE_GLOBAL_PROXY,
                                            WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
}

void waybar::modules::Wireplumber::loadRequiredApiModules() {
  g_autoptr(GError) error = NULL;

  if (!wp_core_load_component(wp_core_, "libwireplumber-module-default-nodes-api", "module", NULL,
                              &error)) {
    throw std::runtime_error(error->message);
  }

  if (!wp_core_load_component(wp_core_, "libwireplumber-module-mixer-api", "module", NULL,
                              &error)) {
    throw std::runtime_error(error->message);
  }

  g_ptr_array_add(apis_, wp_plugin_find(wp_core_, "default-nodes-api"));
  g_ptr_array_add(apis_, ({
                    WpPlugin* p = wp_plugin_find(wp_core_, "mixer-api");
                    g_object_set(G_OBJECT(p), "scale", 1 /* cubic */, NULL);
                    p;
                  }));
}

auto waybar::modules::Wireplumber::update() -> void {
  auto format = config_["format"].isString() ? config_["format"].asString() : format_;
  std::string sinkformat =
      fmt::format(config_["format-sink"].isString() ? config_["format-sink"].asString()
                                                    : std::string("OUT: {volume}%"),
                  fmt::arg("volume", sinkvolume_), fmt::arg("sinknode_name", sinknode_name_),
                  fmt::arg("sourcenode_name", sourcenode_name_));
  std::string sourceformat =
      fmt::format(config_["format-source"].isString() ? config_["format-source"].asString()
                                                      : std::string("IN: {volume}%"),
                  fmt::arg("volume", sourcevolume_), fmt::arg("sinknode_name", sinknode_name_),
                  fmt::arg("sourcenode_name", sourcenode_name_));

  std::string tooltip_format;

  if (sinkmuted_) {
    if (config_["format-sink-muted"].isString()) {
      sinkformat = config_["format-sink-muted"].asString();
    }
    label_.get_style_context()->add_class("sinkmuted");
  } else {
    label_.get_style_context()->remove_class("sinkmuted");
  }
  if (sourcemuted_) {
    if (config_["format-source-muted"].isString()) {
      sourceformat = config_["format-source-muted"].asString();
    }
    label_.get_style_context()->add_class("sourcemuted");
  } else {
    label_.get_style_context()->remove_class("sourcemuted");
  }
  std::string markup = fmt::format(format, fmt::arg("format_sink", sinkformat),
                                   fmt::arg("format_source", sourceformat));
  label_.set_markup(markup);

  getState(sinkvolume_);
  getState(sourcevolume_);

  if (tooltipEnabled()) {
    if (tooltip_format.empty() && config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }

    if (!tooltip_format.empty()) {
      label_.set_tooltip_text(fmt::format(tooltip_format, fmt::arg("sinknode_name", sinknode_name_),
                                          fmt::arg("sourcenode_name", sourcenode_name_),
                                          fmt::arg("sinkvolume", sinkformat),
                                          fmt::arg("sourcevolume", sourceformat)));
    } else {
      label_.set_tooltip_text(fmt::format("Sink: {} Source: {}", sinknode_name_, sourcenode_name_));
    }
  }

  // Call parent update
  ALabel::update();
}
