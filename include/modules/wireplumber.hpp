#pragma once

#include <fmt/format.h>
#include <wp/wp.h>

#include <algorithm>
#include <array>

#include "ALabel.hpp"

namespace waybar::modules {

class Wireplumber : public ALabel {
 public:
  Wireplumber(const std::string&, const Json::Value&);
  virtual ~Wireplumber();
  auto update() -> void override;

 private:
  void asyncLoadRequiredApiModules();
  void prepare(waybar::modules::Wireplumber* self);
  void activatePlugins();
  static void updateVolume(waybar::modules::Wireplumber* self, uint32_t id);
  static void updateNodeName(waybar::modules::Wireplumber* self, uint32_t id);
  static void onPluginActivated(WpObject* p, GAsyncResult* res, waybar::modules::Wireplumber* self);
  static void onDefaultNodesApiLoaded(WpObject* p, GAsyncResult* res,
                                      waybar::modules::Wireplumber* self);
  static void onMixerApiLoaded(WpObject* p, GAsyncResult* res, waybar::modules::Wireplumber* self);
  static void onObjectManagerInstalled(waybar::modules::Wireplumber* self);
  static void onMixerChanged(waybar::modules::Wireplumber* self, uint32_t id);
  static void onDefaultNodesApiChanged(waybar::modules::Wireplumber* self);

  bool handleScroll(GdkEventScroll* e) override;

  static std::list<waybar::modules::Wireplumber*> modules;

  WpCore* wp_core_;
  GPtrArray* apis_;
  WpObjectManager* om_;
  WpPlugin* mixer_api_;
  WpPlugin* def_nodes_api_;
  gchar* default_node_name_;
  uint32_t pending_plugins_;
  bool muted_;
  double volume_;
  double min_step_;
  uint32_t node_id_{0};
  std::string node_name_;
  gchar* type_;
};

}  // namespace waybar::modules
