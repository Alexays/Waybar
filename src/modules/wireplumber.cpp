#include "modules/wireplumber.hpp"
#include <unordered_set>

#include <spdlog/spdlog.h>

bool isValidNodeId(uint32_t id) { return id > 0 && id < G_MAXUINT32; }

std::list<waybar::modules::Wireplumber*> waybar::modules::Wireplumber::modules;

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
      node_id_(0),
      node_name_(""),
      source_name_(""),
      type_(nullptr),
      source_node_id_(0),
      source_muted_(false),
      source_volume_(0.0),
      default_source_name_(nullptr),
      only_physical_(false) {
  waybar::modules::Wireplumber::modules.push_back(this);

  wp_init(WP_INIT_PIPEWIRE);
  wp_core_ = wp_core_new(nullptr, nullptr, nullptr);
  apis_ = g_ptr_array_new_with_free_func(g_object_unref);
  om_ = wp_object_manager_new();

  type_ = g_strdup(config_["node-type"].isString() ? config_["node-type"].asString().c_str()
                                                   : "Audio/Sink");
  only_physical_ = config_["only-physical"].isBool() ? config_["only-physical"].asBool() : false;

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
  uint32_t defaultNodeId = 0;
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-node", self->type_, &defaultNodeId);

  if (isValidNodeId(defaultNodeId)) {
    uint32_t effective_id = self->only_physical_ ? self->resolvePhysicalSink(defaultNodeId) : defaultNodeId;

    if (self->only_physical_ && effective_id != defaultNodeId) {
      spdlog::info("[{}]: only-physical enabled: using sink {} instead of default {}", self->name_, effective_id, defaultNodeId);
    }

    g_autoptr(WpNode) node = static_cast<WpNode*>(
        wp_object_manager_lookup(self->om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id",
                                 "=u", effective_id, nullptr));

    if (node != nullptr) {
      const gchar* effectiveNodeName =
          wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(node), "node.name");

      if (g_strcmp0(self->default_node_name_, effectiveNodeName) != 0 ||
          self->node_id_ != effective_id) {
        spdlog::debug("[{}]: Default sink resolved to -> Node(name: {}, id: {})", self->name_,
                      effectiveNodeName, effective_id);

        g_free(self->default_node_name_);
        self->default_node_name_ = g_strdup(effectiveNodeName);
        self->node_id_ = effective_id;
        updateVolume(self, effective_id);
        updateNodeName(self, effective_id);
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
  uint32_t initial_sink_id = 0;
  g_signal_emit_by_name(self->def_nodes_api_, "get-default-node", self->type_, &initial_sink_id);

  if (self->only_physical_ && isValidNodeId(initial_sink_id)) {
    self->node_id_ = self->resolvePhysicalSink(initial_sink_id);
    spdlog::info("[{}]: only-physical enabled: initial physical sink {} (default was {})",
                 self->name_, self->node_id_, initial_sink_id);
  } else {
    self->node_id_ = initial_sink_id;
  }

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
  if(only_physical_){
    wp_object_manager_add_interest(om_, WP_TYPE_NODE, nullptr);
    wp_object_manager_add_interest(om_, WP_TYPE_LINK, nullptr);
    wp_object_manager_add_interest(om_, WP_TYPE_PORT, nullptr);
    wp_object_manager_request_object_features(om_, WP_TYPE_GLOBAL_PROXY, WP_OBJECT_FEATURES_ALL);
  } else {
    wp_object_manager_add_interest(om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class",
                                   "=s", self->type_, nullptr);
    wp_object_manager_add_interest(om_, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class",
                                   "=s", "Audio/Source", nullptr);
  }
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
      label_.set_tooltip_text(fmt::format(
          fmt::runtime(tooltipFormat), fmt::arg("node_name", node_name_), fmt::arg("volume", vol),
          fmt::arg("icon", getIcon(vol)), fmt::arg("format_source", formatted_source),
          fmt::arg("source_volume", source_vol), fmt::arg("source_desc", source_name_)));
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

// Finds the output node for filter chains defined in pipewire,
// since their input nodes are NOT providing actual outputs
uint32_t waybar::modules::Wireplumber::findPlaybackNodeId(const gchar* description) {
  if (!description || *description == '\0') {
    return 0;
  }

  spdlog::debug("[{}]: Searching for playback node with node.description = {}", name_, description);

  g_autoptr(WpIterator) it = wp_object_manager_new_filtered_iterator(
      om_, WP_TYPE_NODE,
      WP_CONSTRAINT_TYPE_PW_PROPERTY, "node.description", "=s", description,
      WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class", "=s", "Stream/Output/Audio",
      nullptr);

  uint32_t playback_id = 0;

  g_auto(GValue) item = G_VALUE_INIT;
  if(wp_iterator_next(it, &item)) {
    WpNode* output_node = WP_NODE(g_value_get_object(&item));
    playback_id = wp_proxy_get_bound_id(WP_PROXY(output_node));

    spdlog::debug("[{}]: Found matching playback node id {}", name_, playback_id);
  }
  g_value_unset(&item);

  if (playback_id == 0) {
    spdlog::debug("[{}]: No playback node found with description '{}'", name_, description);
  }

  return playback_id;
}

uint32_t waybar::modules::Wireplumber::get_linked_sink_id(WpObjectManager* om, uint32_t from_node_id) {
  spdlog::debug("[{}]: Searching for links connected to node {}", name_, from_node_id);

  g_autoptr(WpIterator) out_it = wp_object_manager_new_filtered_iterator(
      om, WP_TYPE_LINK,
      WP_CONSTRAINT_TYPE_PW_PROPERTY, "link.output.node", "=u", from_node_id,
      nullptr);

  g_auto(GValue) item = G_VALUE_INIT;
  if(wp_iterator_next(out_it, &item)) {
    WpLink* link = WP_LINK(g_value_get_object(&item));
    guint32 out_node, out_port, in_node, in_port;
    wp_link_get_linked_object_ids(link, &out_node, &out_port, &in_node, &in_port);

    spdlog::debug("[{}]: Found outgoing link {} -> {}", name_, out_node, in_node);

    g_value_unset(&item);
    return in_node;
  }
  g_value_unset(&item);

  spdlog::debug("[{}]: No links found connected to node {}", name_, from_node_id);
  return 0;
}

// Follow non-monitor output ports to the next node
uint32_t waybar::modules::Wireplumber::get_linked_node_from_output_ports(WpObjectManager* om, uint32_t from_node_id) {
  spdlog::debug("[{}]: Searching for non-monitor output ports on node {}", name_, from_node_id);

  g_autoptr(WpIterator) port_it = wp_object_manager_new_filtered_iterator(
      om, WP_TYPE_PORT,
      WP_CONSTRAINT_TYPE_PW_PROPERTY, "node.id", "=u", from_node_id,
      WP_CONSTRAINT_TYPE_PW_PROPERTY, "port.direction", "=s", "out",
      nullptr);

  g_auto(GValue) port_item = G_VALUE_INIT;
  while (wp_iterator_next(port_it, &port_item)) {
    WpPort* port = WP_PORT(g_value_get_object(&port_item));

    g_autoptr(WpProperties) port_props = wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(port));
    if (!port_props) {
      g_value_unset(&port_item);
      continue;
    }

    const gchar* name = wp_properties_get(port_props, "port.name");

    // WP_CONSTRAINT_VERB_MATCHES uses GPatternSpec and it is glob-like. Unfortunately, there is no way to
    // express "not beginning with a string" in glob-style regex. Or at least I didn't figure out how to do that.
    // Therefore, just filter them out with a conditional. Performance difference should be negligible anyway.
    if (!name || g_str_has_prefix(name, "monitor_")) {
      g_value_unset(&port_item);
      continue;
    }

    spdlog::debug("[{}]: Found non-monitor output port with name '{}'", name_, name);

    // Find outgoing link from this port
    uint32_t port_id = wp_proxy_get_bound_id(WP_PROXY(port));

    g_autoptr(WpIterator) link_it = wp_object_manager_new_filtered_iterator(
        om, WP_TYPE_LINK,
        WP_CONSTRAINT_TYPE_PW_PROPERTY, "link.output.port", "=u", port_id,
        nullptr);

    g_auto(GValue) link_item = G_VALUE_INIT;
    if (wp_iterator_next(link_it, &link_item)) {
      WpLink* link = WP_LINK(g_value_get_object(&link_item));
      guint32 out_node, out_port, in_node, in_port;
      wp_link_get_linked_object_ids(link, &out_node, &out_port, &in_node, &in_port);

      spdlog::debug("[{}]: Found link from port {} (node {}) -> node {}", name_, port_id, from_node_id, in_node);
      g_value_unset(&link_item);
      g_value_unset(&port_item);
      return in_node;
    }
    g_value_unset(&link_item);

    g_value_unset(&port_item);
  }
  g_value_unset(&port_item);

  spdlog::debug("[{}]: No non-monitor output ports with links on node {}", name_, from_node_id);
  return 0;
}

uint32_t waybar::modules::Wireplumber::resolvePhysicalSink(uint32_t start_id) {
  if (!isValidNodeId(start_id) || !only_physical_) {
    return start_id;
  }

  std::unordered_set<uint32_t> visited;
  uint32_t current_id = start_id;
  int depth = 0;
  const int max_depth = 10;

  spdlog::debug("[{}]: Starting physical sink resolution from id {}", name_, start_id);

  // Follow the output node chain until a physical device is found
  while (visited.insert(current_id).second && depth++ < max_depth) {
    g_autoptr(WpProxy) proxy = static_cast<WpProxy*>(wp_object_manager_lookup(
        om_, WP_TYPE_GLOBAL_PROXY,
        WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id", "=u", current_id,
        nullptr));

    if (!proxy || !WP_IS_PIPEWIRE_OBJECT(proxy)) {
      spdlog::warn("[{}]: Node {} not found during resolution", name_, current_id);
      break;
    }

    // 1: If it has a device.id, we found the physical sink
    g_autoptr(WpProperties) props = wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(proxy));
    if (!props) props = wp_properties_new_empty();

    const gchar* device_id = wp_properties_get(props, "device.id");
    if (device_id != nullptr) {
      spdlog::debug("[{}]: Found physical sink {} (device.id = {})", name_, current_id, device_id);
      break;
    }

    spdlog::debug("[{}]: Node {} is virtual, trying direct output ports", name_, current_id);

    // 2: Try following non-monitor output ports
    uint32_t next_id = get_linked_node_from_output_ports(om_, current_id);
    if (next_id != 0) {
      spdlog::debug("[{}]: Following direct output port link to node {}", name_, next_id);
      current_id = next_id;
      continue;
    }

    // 3: Search for audio stream node
    // (pipewire filter chains create a node for input and a separate node for output)
    const gchar* description = wp_properties_get(props, "node.description");
    if (!description || *description == '\0') {
      spdlog::warn("[{}]: Virtual node {} has no description/nick - cannot search playback node", name_, current_id);
      break;
    }

    spdlog::debug("[{}]: No direct output ports, searching playback node for description '{}'", name_, description);

    uint32_t playback_id = findPlaybackNodeId(description);
    if (playback_id == 0) {
      spdlog::warn("[{}]: No playback node found for virtual sink {} - stopping at virtual sink", name_, current_id);
      break;
    }

    next_id = get_linked_sink_id(om_, playback_id);
    if (next_id == 0) {
      spdlog::warn("[{}]: Playback node {} has no outgoing links - stopping at virtual sink {}", name_, playback_id, current_id);
      break;
    }

    spdlog::debug("[{}]: Following playback node link to node {}", name_, next_id);
    current_id = next_id;
  }


  GVariant* variant = nullptr;
  g_signal_emit_by_name(mixer_api_, "get-volume", current_id, &variant);

  if (variant == nullptr) {
    spdlog::warn("[{}]: Node {} does not support volume - fallback to default sink id", name_, current_id);
    current_id = start_id;
  }
  spdlog::info("[{}]: Final resolved sink id {}", name_, current_id);
  return current_id;
}
