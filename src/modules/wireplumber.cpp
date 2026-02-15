#include "modules/wireplumber.hpp"

#include <spdlog/spdlog.h>

bool isValidNodeId(uint32_t id) { return id > 0 && id < G_MAXUINT32; }

std::list<waybar::modules::Wireplumber*> waybar::modules::Wireplumber::modules;

waybar::modules::Wireplumber::Wireplumber(const std::string& id, const Json::Value& config,
                                          std::mutex& reap_mtx, std::list<pid_t>& reap)
    : ALabel(config, "wireplumber", id, "{volume}%", reap_mtx, reap),
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
      node_id_(0),
      node_name_(""),
      source_name_(""),
      type_(nullptr),
      source_node_id_(0),
      source_muted_(false),
      source_volume_(0.0),
      default_source_name_(nullptr) {
  waybar::modules::Wireplumber::modules.push_back(this);

  wp_init(WP_INIT_PIPEWIRE);
  wp_core_ = wp_core_new(nullptr, nullptr, nullptr);
  apis_ = g_ptr_array_new_with_free_func(g_object_unref);
  om_ = wp_object_manager_new();

  type_ = g_strdup(config_["node-type"].isString() ? config_["node-type"].asString().c_str()
                                                   : "Audio/Sink");

  prepare(this);

  spdlog::debug("[{}]: connecting to pipewire: '{}'...", name_, type_);

  if (wp_core_connect(wp_core_) == 0) {
    spdlog::error("[{}]: Could not connect to PipeWire: '{}'", name_, type_);
    throw std::runtime_error("Could not connect to PipeWire\n");
  }

  spdlog::debug("[{}]: {} connected!", name_, type_);

  g_signal_connect_swapped(om_, "installed", (GCallback)onObjectManagerInstalled, this);

  asyncLoadRequiredApiModules();
}

waybar::modules::Wireplumber::~Wireplumber() {
  waybar::modules::Wireplumber::modules.remove(this);
  wp_core_disconnect(wp_core_);
  g_clear_pointer(&apis_, g_ptr_array_unref);
  g_clear_object(&om_);
  g_clear_object(&wp_core_);
  g_clear_object(&mixer_api_);
  g_clear_object(&def_nodes_api_);
  g_free(default_node_name_);
  g_free(default_source_name_);
  g_free(type_);
}

void waybar::modules::Wireplumber::updateNodeName(waybar::modules::Wireplumber* self, uint32_t id) {
  spdlog::debug("[{}]: updating '{}' node name with node.id {}", self->name_, self->type_, id);

  if (!isValidNodeId(id)) {
    spdlog::warn("[{}]: '{}' is not a valid node ID. Ignoring '{}' node name update.", self->name_,
                 id, self->type_);
    return;
  }

  auto* proxy = static_cast<WpProxy*>(wp_object_manager_lookup(self->om_, WP_TYPE_GLOBAL_PROXY,
                                                               WP_CONSTRAINT_TYPE_G_PROPERTY,
                                                               "bound-id", "=u", id, nullptr));

  if (proxy == nullptr) {
    auto err = fmt::format("Object '{}' not found\n", id);
    spdlog::error("[{}]: {}", self->name_, err);
    throw std::runtime_error(err);
  }

  g_autoptr(WpProperties) properties =
      WP_IS_PIPEWIRE_OBJECT(proxy) != 0
          ? wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(proxy))
          : wp_properties_new_empty();
  g_autoptr(WpProperties) globalP = wp_global_proxy_get_global_properties(WP_GLOBAL_PROXY(proxy));
  properties = wp_properties_ensure_unique_owner(properties);
  wp_properties_add(properties, globalP);
  wp_properties_set(properties, "object.id", nullptr);
  const auto* nick = wp_properties_get(properties, "node.nick");
  const auto* description = wp_properties_get(properties, "node.description");

  self->node_name_ = nick != nullptr          ? nick
                     : description != nullptr ? description
                                              : "Unknown node name";
  spdlog::debug("[{}]: Updating '{}' node name to: {}", self->name_, self->type_, self->node_name_);
}

void waybar::modules::Wireplumber::updateSourceName(waybar::modules::Wireplumber* self,
                                                    uint32_t id) {
  spdlog::debug("[{}]: updating source name with node.id {}", self->name_, id);

  if (!isValidNodeId(id)) {
    spdlog::warn("[{}]: '{}' is not a valid source node ID. Ignoring source name update.",
                 self->name_, id);
    return;
  }

  auto* proxy = static_cast<WpProxy*>(wp_object_manager_lookup(self->om_, WP_TYPE_GLOBAL_PROXY,
                                                               WP_CONSTRAINT_TYPE_G_PROPERTY,
                                                               "bound-id", "=u", id, nullptr));

  if (proxy == nullptr) {
    auto err = fmt::format("Source object '{}' not found\n", id);
    spdlog::error("[{}]: {}", self->name_, err);
    return;
  }

  g_autoptr(WpProperties) properties =
      WP_IS_PIPEWIRE_OBJECT(proxy) != 0
          ? wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(proxy))
          : wp_properties_new_empty();
  g_autoptr(WpProperties) globalP = wp_global_proxy_get_global_properties(WP_GLOBAL_PROXY(proxy));
  properties = wp_properties_ensure_unique_owner(properties);
  wp_properties_add(properties, globalP);
  wp_properties_set(properties, "object.id", nullptr);
  const auto* nick = wp_properties_get(properties, "node.nick");
  const auto* description = wp_properties_get(properties, "node.description");

  self->source_name_ = nick != nullptr          ? nick
                       : description != nullptr ? description
                                                : "Unknown source name";
  spdlog::debug("[{}]: Updating source name to: {}", self->name_, self->source_name_);
}

void waybar::modules::Wireplumber::updateVolume(waybar::modules::Wireplumber* self, uint32_t id) {
  spdlog::debug("[{}]: updating volume", self->name_);
  GVariant* variant = nullptr;

  if (!isValidNodeId(id)) {
    spdlog::error("[{}]: '{}' is not a valid '{}' node ID. Ignoring volume update.", self->name_,
                  id, self->type_);
    return;
  }

  g_signal_emit_by_name(self->mixer_api_, "get-volume", id, &variant);

  if (variant == nullptr) {
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

void waybar::modules::Wireplumber::updateSourceVolume(waybar::modules::Wireplumber* self,
                                                      uint32_t id) {
  spdlog::debug("[{}]: updating source volume", self->name_);
  GVariant* variant = nullptr;

  if (!isValidNodeId(id)) {
    spdlog::error("[{}]: '{}' is not a valid source node ID. Ignoring source volume update.",
                  self->name_, id);
    return;
  }

  g_signal_emit_by_name(self->mixer_api_, "get-volume", id, &variant);

  if (variant == nullptr) {
    spdlog::debug("[{}]: Source node {} does not support volume", self->name_, id);
    return;
  }

  g_variant_lookup(variant, "volume", "d", &self->source_volume_);
  g_variant_lookup(variant, "mute", "b", &self->source_muted_);
  g_clear_pointer(&variant, g_variant_unref);

  self->dp.emit();
}

void waybar::modules::Wireplumber::onMixerChanged(waybar::modules::Wireplumber* self, uint32_t id) {
  g_autoptr(WpNode) node = static_cast<WpNode*>(wp_object_manager_lookup(
      self->om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id", "=u", id, nullptr));

  if (node == nullptr) {
    // log a warning only if no other widget is targeting the id.
    // this reduces log spam when multiple instances of the module are used on different node types.
    if (id != self->node_id_ && id != self->source_node_id_) {
      for (auto const& module : waybar::modules::Wireplumber::modules) {
        if (module->node_id_ == id || module->source_node_id_ == id) {
          return;
        }
      }
    }

    spdlog::warn("[{}]: (onMixerChanged: {}) - Object with id {} not found", self->name_,
                 self->type_, id);
    return;
  }

  const gchar* name = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(node), "node.name");

  if (self->node_id_ == id) {
    spdlog::debug("[{}]: (onMixerChanged: {}) - updating sink volume for node: {}", self->name_,
                  self->type_, name);
    updateVolume(self, id);
  } else if (self->source_node_id_ == id) {
    spdlog::debug("[{}]: (onMixerChanged: {}) - updating source volume for node: {}", self->name_,
                  self->type_, name);
    updateSourceVolume(self, id);
  }
}

void waybar::modules::Wireplumber::onDefaultNodesApiChanged(waybar::modules::Wireplumber* self) {
  spdlog::debug("[{}]: (onDefaultNodesApiChanged: {})", self->name_, self->type_);

  // Handle sink
  uint32_t defaultNodeId;
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-node", self->type_, &defaultNodeId);

  if (isValidNodeId(defaultNodeId)) {
    g_autoptr(WpNode) node = static_cast<WpNode*>(
        wp_object_manager_lookup(self->om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id",
                                 "=u", defaultNodeId, nullptr));

    if (node != nullptr) {
      const gchar* defaultNodeName =
          wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(node), "node.name");

      if (g_strcmp0(self->default_node_name_, defaultNodeName) != 0 ||
          self->node_id_ != defaultNodeId) {
        spdlog::debug("[{}]: Default sink changed to -> Node(name: {}, id: {})", self->name_,
                      defaultNodeName, defaultNodeId);

        g_free(self->default_node_name_);
        self->default_node_name_ = g_strdup(defaultNodeName);
        self->node_id_ = defaultNodeId;
        updateVolume(self, defaultNodeId);
        updateNodeName(self, defaultNodeId);
      }
    }
  }

  // Handle source
  uint32_t defaultSourceId;
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-node", "Audio/Source", &defaultSourceId);

  if (isValidNodeId(defaultSourceId)) {
    g_autoptr(WpNode) sourceNode = static_cast<WpNode*>(
        wp_object_manager_lookup(self->om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id",
                                 "=u", defaultSourceId, nullptr));

    if (sourceNode != nullptr) {
      const gchar* defaultSourceName =
          wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(sourceNode), "node.name");

      if (g_strcmp0(self->default_source_name_, defaultSourceName) != 0 ||
          self->source_node_id_ != defaultSourceId) {
        spdlog::debug("[{}]: Default source changed to -> Node(name: {}, id: {})", self->name_,
                      defaultSourceName, defaultSourceId);

        g_free(self->default_source_name_);
        self->default_source_name_ = g_strdup(defaultSourceName);
        self->source_node_id_ = defaultSourceId;
        updateSourceVolume(self, defaultSourceId);
        updateSourceName(self, defaultSourceId);
      }
    }
  }
}

void waybar::modules::Wireplumber::onObjectManagerInstalled(waybar::modules::Wireplumber* self) {
  spdlog::debug("[{}]: onObjectManagerInstalled", self->name_);

  self->def_nodes_api_ = wp_plugin_find(self->wp_core_, "default-nodes-api");

  if (self->def_nodes_api_ == nullptr) {
    spdlog::error("[{}]: default nodes api is not loaded.", self->name_);
    throw std::runtime_error("Default nodes API is not loaded\n");
  }

  self->mixer_api_ = wp_plugin_find(self->wp_core_, "mixer-api");

  if (self->mixer_api_ == nullptr) {
    spdlog::error("[{}]: mixer api is not loaded.", self->name_);
    throw std::runtime_error("Mixer api is not loaded\n");
  }

  // Get default sink
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-configured-node-name", self->type_,
                        &self->default_node_name_);
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-node", self->type_, &self->node_id_);

  // Get default source
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-configured-node-name", "Audio/Source",
                        &self->default_source_name_);
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-node", "Audio/Source",
                        &self->source_node_id_);

  if (self->default_node_name_ != nullptr) {
    spdlog::debug(
        "[{}]: (onObjectManagerInstalled: {}) - default configured node name: {} and id: {}",
        self->name_, self->type_, self->default_node_name_, self->node_id_);
  }
  if (self->default_source_name_ != nullptr) {
    spdlog::debug("[{}]: default source: {} (id: {})", self->name_, self->default_source_name_,
                  self->source_node_id_);
  }

  updateVolume(self, self->node_id_);
  updateNodeName(self, self->node_id_);
  updateSourceVolume(self, self->source_node_id_);
  updateSourceName(self, self->source_node_id_);

  g_signal_connect_swapped(self->mixer_api_, "changed", (GCallback)onMixerChanged, self);
  g_signal_connect_swapped(self->def_nodes_api_, "changed", (GCallback)onDefaultNodesApiChanged,
                           self);
}

void waybar::modules::Wireplumber::onPluginActivated(WpObject* p, GAsyncResult* res,
                                                     waybar::modules::Wireplumber* self) {
  const auto* pluginName = wp_plugin_get_name(WP_PLUGIN(p));
  spdlog::debug("[{}]: onPluginActivated: {}", self->name_, pluginName);
  g_autoptr(GError) error = nullptr;

  if (wp_object_activate_finish(p, res, &error) == 0) {
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
    wp_object_activate(WP_OBJECT(plugin), WP_PLUGIN_FEATURE_ENABLED, nullptr,
                       (GAsyncReadyCallback)onPluginActivated, this);
  }
}

void waybar::modules::Wireplumber::prepare(waybar::modules::Wireplumber* self) {
  spdlog::debug("[{}]: preparing object manager: '{}'", name_, self->type_);
  wp_object_manager_add_interest(om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class",
                                 "=s", self->type_, nullptr);
  wp_object_manager_add_interest(om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class",
                                 "=s", "Audio/Source", nullptr);
}

void waybar::modules::Wireplumber::onDefaultNodesApiLoaded(WpObject* p, GAsyncResult* res,
                                                           waybar::modules::Wireplumber* self) {
  gboolean success = FALSE;
  g_autoptr(GError) error = nullptr;

  spdlog::debug("[{}]: callback loading default node api module", self->name_);

  success = wp_core_load_component_finish(self->wp_core_, res, &error);

  if (success == FALSE) {
    spdlog::error("[{}]: default nodes API load failed", self->name_);
    throw std::runtime_error(error->message);
  }
  spdlog::debug("[{}]: loaded default nodes api", self->name_);
  g_ptr_array_add(self->apis_, wp_plugin_find(self->wp_core_, "default-nodes-api"));

  spdlog::debug("[{}]: loading mixer api module", self->name_);
  wp_core_load_component(self->wp_core_, "libwireplumber-module-mixer-api", "module", nullptr,
                         "mixer-api", nullptr, (GAsyncReadyCallback)onMixerApiLoaded, self);
}

void waybar::modules::Wireplumber::onMixerApiLoaded(WpObject* p, GAsyncResult* res,
                                                    waybar::modules::Wireplumber* self) {
  gboolean success = FALSE;
  g_autoptr(GError) error = nullptr;

  success = wp_core_load_component_finish(self->wp_core_, res, &error);

  if (success == FALSE) {
    spdlog::error("[{}]: mixer API load failed", self->name_);
    throw std::runtime_error(error->message);
  }

  spdlog::debug("[{}]: loaded mixer API", self->name_);
  g_ptr_array_add(self->apis_, ({
                    WpPlugin* p = wp_plugin_find(self->wp_core_, "mixer-api");
                    g_object_set(G_OBJECT(p), "scale", 1 /* cubic */, nullptr);
                    p;
                  }));

  self->activatePlugins();

  self->dp.emit();

  self->event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  self->event_box_.signal_scroll_event().connect(sigc::mem_fun(*self, &Wireplumber::handleScroll));
}

void waybar::modules::Wireplumber::asyncLoadRequiredApiModules() {
  spdlog::debug("[{}]: loading default nodes api module", name_);
  wp_core_load_component(wp_core_, "libwireplumber-module-default-nodes-api", "module", nullptr,
                         "default-nodes-api", nullptr, (GAsyncReadyCallback)onDefaultNodesApiLoaded,
                         this);
}

auto waybar::modules::Wireplumber::update() -> void {
  auto format = format_;
  std::string tooltipFormat;

  // Handle sink mute state
  if (muted_) {
    format = config_["format-muted"].isString() ? config_["format-muted"].asString() : format;
    label_.get_style_context()->add_class("muted");
    label_.get_style_context()->add_class("sink-muted");
  } else {
    label_.get_style_context()->remove_class("muted");
    label_.get_style_context()->remove_class("sink-muted");
  }

  // Handle source mute state
  if (source_muted_) {
    label_.get_style_context()->add_class("source-muted");
  } else {
    label_.get_style_context()->remove_class("source-muted");
  }

  int vol = round(volume_ * 100.0);
  int source_vol = round(source_volume_ * 100.0);

  // Get the state and apply state-specific format if available
  auto state = getState(vol);
  if (!state.empty()) {
    std::string format_name = muted_ ? "format-muted" : "format";
    std::string state_format_name = format_name + "-" + state;
    if (config_[state_format_name].isString()) {
      format = config_[state_format_name].asString();
    }
  }

  // Prepare source format string (similar to PulseAudio)
  std::string format_source = "{volume}%";
  if (source_muted_) {
    if (config_["format-source-muted"].isString()) {
      format_source = config_["format-source-muted"].asString();
    }
  } else {
    if (config_["format-source"].isString()) {
      format_source = config_["format-source"].asString();
    }
  }

  // Format the source string with actual volume
  std::string formatted_source =
      fmt::format(fmt::runtime(format_source), fmt::arg("volume", source_vol));

  std::string markup =
      fmt::format(fmt::runtime(format), fmt::arg("node_name", node_name_), fmt::arg("volume", vol),
                  fmt::arg("icon", getIcon(vol)), fmt::arg("format_source", formatted_source),
                  fmt::arg("source_volume", source_vol), fmt::arg("source_desc", source_name_));
  label_.set_markup(markup);

  if (tooltipEnabled()) {
    if (tooltipFormat.empty() && config_["tooltip-format"].isString()) {
      tooltipFormat = config_["tooltip-format"].asString();
    }

    if (!tooltipFormat.empty()) {
      label_.set_tooltip_markup(fmt::format(
          fmt::runtime(tooltipFormat), fmt::arg("node_name", node_name_), fmt::arg("volume", vol),
          fmt::arg("icon", getIcon(vol)), fmt::arg("format_source", formatted_source),
          fmt::arg("source_volume", source_vol), fmt::arg("source_desc", source_name_)));
    } else {
      label_.set_tooltip_markup(node_name_);
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
  double maxVolume = 1;
  double step = 1.0 / 100.0;
  if (config_["scroll-step"].isDouble()) {
    step = config_["scroll-step"].asDouble() / 100.0;
  }
  if (config_["max-volume"].isDouble()) {
    maxVolume = config_["max-volume"].asDouble() / 100.0;
  }

  if (step < min_step_) step = min_step_;

  double newVol = volume_;
  if (dir == SCROLL_DIR::UP) {
    if (volume_ < maxVolume) {
      newVol = volume_ + step;
      if (newVol > maxVolume) newVol = maxVolume;
    }
  } else if (dir == SCROLL_DIR::DOWN) {
    if (volume_ > 0) {
      newVol = volume_ - step;
      if (newVol < 0) newVol = 0;
    }
  }
  if (newVol != volume_) {
    GVariant* variant = g_variant_new_double(newVol);
    gboolean ret;
    g_signal_emit_by_name(mixer_api_, "set-volume", node_id_, variant, &ret);
  }
  return true;
}
