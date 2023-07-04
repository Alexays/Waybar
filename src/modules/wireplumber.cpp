#include "modules/wireplumber.hpp"

#include <spdlog/spdlog.h>

bool isValidNodeId(uint32_t id) { return id > 0 && id < G_MAXUINT32; }

waybar::modules::Wireplumber::Wireplumber(const std::string& id, const Json::Value& config)
    : ALabel(config, "wireplumber", id, "{volume}%"),
      wp_core_(nullptr),
      apis_(nullptr),
      om_(nullptr),
      mixer_api_(nullptr),
      def_nodes_api_(nullptr),
      default_node_name_(nullptr),
      pending_plugins_(0),
      muted_(false),
      volume_(0.0),
      min_step_(0.0),
      node_id_(0) {
  wp_init(WP_INIT_PIPEWIRE);
  wp_core_ = wp_core_new(NULL, NULL);
  apis_ = g_ptr_array_new_with_free_func(g_object_unref);
  om_ = wp_object_manager_new();

  prepare();

  loadRequiredApiModules();

  spdlog::debug("[{}]: connecting to pipewire...", this->name_);

  if (!wp_core_connect(wp_core_)) {
    spdlog::error("[{}]: Could not connect to PipeWire", this->name_);
    throw std::runtime_error("Could not connect to PipeWire\n");
  }

  spdlog::debug("[{}]: connected!", this->name_);

  g_signal_connect_swapped(om_, "installed", (GCallback)onObjectManagerInstalled, this);

  activatePlugins();

  dp.emit();

  event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &Wireplumber::handleScroll));
}

waybar::modules::Wireplumber::~Wireplumber() {
  g_clear_pointer(&apis_, g_ptr_array_unref);
  g_clear_object(&om_);
  g_clear_object(&wp_core_);
  g_clear_object(&mixer_api_);
  g_clear_object(&def_nodes_api_);
  g_free(default_node_name_);
}

void waybar::modules::Wireplumber::updateNodeName(waybar::modules::Wireplumber* self, uint32_t id) {
  spdlog::debug("[{}]: updating node name with node.id {}", self->name_, id);

  if (!isValidNodeId(id)) {
    spdlog::warn("[{}]: '{}' is not a valid node ID. Ignoring node name update.", self->name_, id);
    return;
  }

  auto proxy = static_cast<WpProxy*>(wp_object_manager_lookup(
      self->om_, WP_TYPE_GLOBAL_PROXY, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id", "=u", id, NULL));

  if (!proxy) {
    auto err = fmt::format("Object '{}' not found\n", id);
    spdlog::error("[{}]: {}", self->name_, err);
    throw std::runtime_error(err);
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

  self->node_name_ = nick ? nick : description;
  spdlog::debug("[{}]: Updating node name to: {}", self->name_, self->node_name_);
}

void waybar::modules::Wireplumber::updateVolume(waybar::modules::Wireplumber* self, uint32_t id) {
  spdlog::debug("[{}]: updating volume", self->name_);
  GVariant* variant = NULL;

  if (!isValidNodeId(id)) {
    spdlog::error("[{}]: '{}' is not a valid node ID. Ignoring volume update.", self->name_, id);
    return;
  }

  g_signal_emit_by_name(self->mixer_api_, "get-volume", id, &variant);

  if (!variant) {
    auto err = fmt::format("Node {} does not support volume\n", id);
    spdlog::error("[{}]: {}", self->name_, err);
    throw std::runtime_error(err);
  }

  g_variant_lookup(variant, "volume", "d", &self->volume_);
  g_variant_lookup(variant, "step", "d", &self->min_step_);
  g_variant_lookup(variant, "mute", "b", &self->muted_);
  g_clear_pointer(&variant, g_variant_unref);

  self->dp.emit();
}

void waybar::modules::Wireplumber::onMixerChanged(waybar::modules::Wireplumber* self, uint32_t id) {
  spdlog::debug("[{}]: (onMixerChanged) - id: {}", self->name_, id);

  g_autoptr(WpNode) node = static_cast<WpNode*>(wp_object_manager_lookup(
      self->om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id", "=u", id, NULL));

  if (!node) {
    spdlog::warn("[{}]: (onMixerChanged) - Object with id {} not found", self->name_, id);
    return;
  }

  const gchar* name = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(node), "node.name");

  if (self->node_id_ != id) {
    spdlog::debug(
        "[{}]: (onMixerChanged) - ignoring mixer update for node: id: {}, name: {} as it is not "
        "the default node: {} with id: {}",
        self->name_, id, name, self->default_node_name_, self->node_id_);
    return;
  }

  spdlog::debug("[{}]: (onMixerChanged) - Need to update volume for node with id {} and name {}",
                self->name_, id, name);
  updateVolume(self, id);
}

void waybar::modules::Wireplumber::onDefaultNodesApiChanged(waybar::modules::Wireplumber* self) {
  spdlog::debug("[{}]: (onDefaultNodesApiChanged)", self->name_);

  uint32_t default_node_id;
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-node", "Audio/Sink", &default_node_id);

  if (!isValidNodeId(default_node_id)) {
    spdlog::warn("[{}]: '{}' is not a valid node ID. Ignoring node change.", self->name_,
                 default_node_id);
    return;
  }

  g_autoptr(WpNode) node = static_cast<WpNode*>(
      wp_object_manager_lookup(self->om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id",
                               "=u", default_node_id, NULL));

  if (!node) {
    spdlog::warn("[{}]: (onDefaultNodesApiChanged) - Object with id {} not found", self->name_,
                 default_node_id);
    return;
  }

  const gchar* default_node_name =
      wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(node), "node.name");

  spdlog::debug(
      "[{}]: (onDefaultNodesApiChanged) - got the following default node: Node(name: {}, id: {})",
      self->name_, default_node_name, default_node_id);

  if (g_strcmp0(self->default_node_name_, default_node_name) == 0) {
    spdlog::debug(
        "[{}]: (onDefaultNodesApiChanged) - Default node has not changed. Node(name: {}, id: {}). "
        "Ignoring.",
        self->name_, self->default_node_name_, default_node_id);
    return;
  }

  spdlog::debug(
      "[{}]: (onDefaultNodesApiChanged) - Default node changed to -> Node(name: {}, id: {})",
      self->name_, default_node_name, default_node_id);

  g_free(self->default_node_name_);
  self->default_node_name_ = g_strdup(default_node_name);
  self->node_id_ = default_node_id;
  updateVolume(self, default_node_id);
  updateNodeName(self, default_node_id);
}

void waybar::modules::Wireplumber::onObjectManagerInstalled(waybar::modules::Wireplumber* self) {
  spdlog::debug("[{}]: onObjectManagerInstalled", self->name_);

  self->def_nodes_api_ = wp_plugin_find(self->wp_core_, "default-nodes-api");

  if (!self->def_nodes_api_) {
    spdlog::error("[{}]: default nodes api is not loaded.", self->name_);
    throw std::runtime_error("Default nodes API is not loaded\n");
  }

  self->mixer_api_ = wp_plugin_find(self->wp_core_, "mixer-api");

  if (!self->mixer_api_) {
    spdlog::error("[{}]: mixer api is not loaded.", self->name_);
    throw std::runtime_error("Mixer api is not loaded\n");
  }

  g_signal_emit_by_name(self->def_nodes_api_, "get-default-configured-node-name", "Audio/Sink",
                        &self->default_node_name_);
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-node", "Audio/Sink", &self->node_id_);

  if (self->default_node_name_) {
    spdlog::debug("[{}]: (onObjectManagerInstalled) - default configured node name: {} and id: {}",
                  self->name_, self->default_node_name_, self->node_id_);
  }

  updateVolume(self, self->node_id_);
  updateNodeName(self, self->node_id_);

  g_signal_connect_swapped(self->mixer_api_, "changed", (GCallback)onMixerChanged, self);
  g_signal_connect_swapped(self->def_nodes_api_, "changed", (GCallback)onDefaultNodesApiChanged,
                           self);
}

void waybar::modules::Wireplumber::onPluginActivated(WpObject* p, GAsyncResult* res,
                                                     waybar::modules::Wireplumber* self) {
  auto plugin_name = wp_plugin_get_name(WP_PLUGIN(p));
  spdlog::debug("[{}]: onPluginActivated: {}", self->name_, plugin_name);
  g_autoptr(GError) error = NULL;

  if (!wp_object_activate_finish(p, res, &error)) {
    spdlog::error("[{}]: error activating plugin: {}", self->name_, error->message);
    throw std::runtime_error(error->message);
  }

  if (--self->pending_plugins_ == 0) {
    wp_core_install_object_manager(self->wp_core_, self->om_);
  }
}

void waybar::modules::Wireplumber::activatePlugins() {
  spdlog::debug("[{}]: activating plugins", name_);
  for (uint16_t i = 0; i < apis_->len; i++) {
    WpPlugin* plugin = static_cast<WpPlugin*>(g_ptr_array_index(apis_, i));
    pending_plugins_++;
    wp_object_activate(WP_OBJECT(plugin), WP_PLUGIN_FEATURE_ENABLED, NULL,
                       (GAsyncReadyCallback)onPluginActivated, this);
  }
}

void waybar::modules::Wireplumber::prepare() {
  spdlog::debug("[{}]: preparing object manager", name_);
  wp_object_manager_add_interest(om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class",
                                 "=s", "Audio/Sink", NULL);
}

void waybar::modules::Wireplumber::loadRequiredApiModules() {
  spdlog::debug("[{}]: loading required modules", name_);
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

  int vol = round(volume_ * 100.0);
  std::string markup = fmt::format(fmt::runtime(format), fmt::arg("node_name", node_name_),
                                   fmt::arg("volume", vol), fmt::arg("icon", getIcon(vol)));
  label_.set_markup(markup);

  getState(vol);

  if (tooltipEnabled()) {
    if (tooltip_format.empty() && config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }

    if (!tooltip_format.empty()) {
      label_.set_tooltip_text(fmt::format(fmt::runtime(tooltip_format),
                                          fmt::arg("node_name", node_name_),
                                          fmt::arg("volume", vol), fmt::arg("icon", getIcon(vol))));
    } else {
      label_.set_tooltip_text(node_name_);
    }
  }

  // Call parent update
  ALabel::update();
}

bool waybar::modules::Wireplumber::handleScroll(GdkEventScroll* e) {
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScroll(e);
  }
  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }
  if (config_["reverse-scrolling"].asInt() == 1) {
    if (dir == SCROLL_DIR::UP) {
      dir = SCROLL_DIR::DOWN;
    } else if (dir == SCROLL_DIR::DOWN) {
      dir = SCROLL_DIR::UP;
    }
  }
  double max_volume = 1;
  double step = 1.0 / 100.0;
  if (config_["scroll-step"].isDouble()) {
    step = config_["scroll-step"].asDouble() / 100.0;
  }
  if (config_["max-volume"].isDouble()) {
    max_volume = config_["max-volume"].asDouble() / 100.0;
  }

  if (step < min_step_) step = min_step_;

  double new_vol = volume_;
  if (dir == SCROLL_DIR::UP) {
    if (volume_ < max_volume) {
      new_vol = volume_ + step;
      if (new_vol > max_volume) new_vol = max_volume;
    }
  } else if (dir == SCROLL_DIR::DOWN) {
    if (volume_ > 0) {
      new_vol = volume_ - step;
      if (new_vol < 0) new_vol = 0;
    }
  }
  if (new_vol != volume_) {
    GVariant* variant = g_variant_new_double(new_vol);
    gboolean ret;
    g_signal_emit_by_name(mixer_api_, "set-volume", node_id_, variant, &ret);
  }
  return true;
}
