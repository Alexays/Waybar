#include "modules/wireplumber.hpp"

waybar::modules::Wireplumber::Wireplumber(const std::string& id, const Json::Value& config)
    : ALabel(config, "wireplumber", id, "{volume}%"),
      wp_core_(nullptr),
      apis_(nullptr),
      om_(nullptr),
      pending_plugins_(0),
      muted_(false),
      volume_(0.0),
      node_id_(0) {
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

uint32_t waybar::modules::Wireplumber::getDefaultNodeId(waybar::modules::Wireplumber* self) {
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

void waybar::modules::Wireplumber::updateNodeName(waybar::modules::Wireplumber* self) {
  auto proxy = static_cast<WpPipewireObject*>(wp_object_manager_lookup(
      self->om_, WP_TYPE_GLOBAL_PROXY, WP_CONSTRAINT_TYPE_PW_GLOBAL_PROPERTY, "object.id", "=u",
      self->node_id_, NULL));

  if (!proxy) {
    throw std::runtime_error(fmt::format("Object '{}' not found\n", self->node_id_));
  }

  g_autoptr(WpProperties) properties = wp_pipewire_object_get_properties(proxy);
  properties = wp_properties_ensure_unique_owner(properties);
  self->node_name_ = wp_properties_get(properties, "node.nick");
}

void waybar::modules::Wireplumber::updateVolume(waybar::modules::Wireplumber* self) {
  double vol;
  GVariant* variant = NULL;
  g_autoptr(WpPlugin) mixer_api = wp_plugin_find(self->wp_core_, "mixer-api");
  g_signal_emit_by_name(mixer_api, "get-volume", self->node_id_, &variant);
  if (!variant) {
    auto err = fmt::format("Node {} does not support volume\n", self->node_id_);
    throw std::runtime_error(err);
  }

  g_variant_lookup(variant, "volume", "d", &vol);
  g_variant_lookup(variant, "mute", "b", &self->muted_);
  g_clear_pointer(&variant, g_variant_unref);

  self->volume_ = std::round(vol * 100.0F);
  self->dp.emit();
}

void waybar::modules::Wireplumber::onObjectManagerInstalled(waybar::modules::Wireplumber* self) {
  self->node_id_ =
      self->config_["node-id"].isInt() ? self->config_["node-id"].asInt() : getDefaultNodeId(self);

  g_autoptr(WpPlugin) mixer_api = wp_plugin_find(self->wp_core_, "mixer-api");

  updateVolume(self);
  updateNodeName(self);
  g_signal_connect_swapped(mixer_api, "changed", (GCallback)updateVolume, self);
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
  auto format = format_;
  std::string tooltip_format;

  if (muted_) {
    format = config_["format-muted"].isString() ? config_["format-muted"].asString() : format;
    label_.get_style_context()->add_class("muted");
  } else {
    label_.get_style_context()->remove_class("muted");
  }

  std::string markup =
      fmt::format(format, fmt::arg("node_name", node_name_), fmt::arg("volume", volume_));
  label_.set_markup(markup);

  getState(volume_);

  if (tooltipEnabled()) {
    if (tooltip_format.empty() && config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }

    if (!tooltip_format.empty()) {
      label_.set_tooltip_text(fmt::format(tooltip_format, fmt::arg("node_name", node_name_),
                                          fmt::arg("volume", volume_)));
    } else {
      label_.set_tooltip_text(node_name_);
    }
  }

  // Call parent update
  ALabel::update();
}
