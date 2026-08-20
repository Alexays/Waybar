#pragma once

#include <fmt/format.h>
#include <wp/wp.h>

#include <algorithm>
#include <array>

#include "ALabel.hpp"

namespace waybar::modules {

class Wireplumber : public ALabel {
 public:
  Wireplumber(const std::string&, const Json::Value&, std::mutex&, std::list<pid_t>&);
  virtual ~Wireplumber();
  auto update() -> void override;

 private:
  bool setupConnection();
  void teardownConnection();
  void scheduleReconnect();
  bool onReconnectTimeout();
  static void onCoreDisconnected(waybar::modules::Wireplumber* self);
  void asyncLoadRequiredApiModules();
  void prepare(waybar::modules::Wireplumber* self);
  void activatePlugins();
  static void updateVolume(waybar::modules::Wireplumber* self, uint32_t id);
  static void updateNodeName(waybar::modules::Wireplumber* self, uint32_t id);
  static void updateSourceVolume(waybar::modules::Wireplumber* self, uint32_t id);
  static void updateSourceName(waybar::modules::Wireplumber* self, uint32_t id);  // NEW
  static void onPluginActivated(WpObject* p, GAsyncResult* res, gpointer data);
  static void onDefaultNodesApiLoaded(WpObject* p, GAsyncResult* res, gpointer data);
  static void onMixerApiLoaded(WpObject* p, GAsyncResult* res, gpointer data);
  static void onObjectManagerInstalled(waybar::modules::Wireplumber* self);
  static void onMixerChanged(waybar::modules::Wireplumber* self, uint32_t id);
  static void onDefaultNodesApiChanged(waybar::modules::Wireplumber* self);

  bool handleScroll(GdkEventScroll* e) override;
  std::vector<std::string> getWPIcon();

  static std::list<waybar::modules::Wireplumber*> modules;
  // Returns true while `self` is still a live module. Async load/activation callbacks use this to
  // avoid dereferencing a `self` that was destroyed before the callback fired (see #3974).
  static bool isModuleAlive(waybar::modules::Wireplumber* self);

  uint32_t resolvePhysicalSink(uint32_t start_id);
  uint32_t findPlaybackNodeId(const gchar* description);
  uint32_t get_linked_sink_id(WpObjectManager* om, uint32_t from_node_id);
  uint32_t get_linked_node_from_output_ports(WpObjectManager* om, uint32_t from_node_id);

  WpCore* wp_core_;
  GPtrArray* apis_;
  WpObjectManager* om_;
  WpPlugin* mixer_api_;
  WpPlugin* def_nodes_api_;
  gchar* default_node_name_;
  uint32_t pending_plugins_;
  // Bumped on every (re)connection. The async load/activate callbacks capture the generation they
  // were scheduled under (via their user_data) and no-op if it no longer matches, so a completion
  // from a connection that was already torn down cannot corrupt the new generation's state (#2882).
  uint32_t connection_generation_{0};
  bool muted_;
  double volume_;
  double min_step_;
  uint32_t node_id_{0};
  std::string node_name_;
  std::string source_name_;
  gchar* type_;
  uint32_t source_node_id_;
  bool source_muted_;
  double source_volume_;
  gchar* default_source_name_;
  bool only_physical_;
  bool resolved_physical_;
  std::string form_factor_;
  // Timer used to retry connecting to PipeWire after it goes away; disconnected in the destructor
  // so a pending attempt can't outlive the module. See #2882.
  sigc::connection reconnect_timer_;
};

}  // namespace waybar::modules
