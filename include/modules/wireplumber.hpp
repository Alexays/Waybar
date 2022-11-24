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
  ~Wireplumber();
  auto update() -> void;

 private:
  void loadRequiredApiModules();
  void prepare();
  void activatePlugins();
  static void updateVolume(waybar::modules::Wireplumber* self);
  static void updateNodeName(waybar::modules::Wireplumber* self);
  static uint32_t getDefaultNodeId(waybar::modules::Wireplumber* self);
  static void onPluginActivated(WpObject* p, GAsyncResult* res, waybar::modules::Wireplumber* self);
  static void onObjectManagerInstalled(waybar::modules::Wireplumber* self);

  WpCore* wp_core_;
  GPtrArray* apis_;
  WpObjectManager* om_;
  uint32_t pending_plugins_;
  bool muted_;
  double volume_;
  uint32_t node_id_{0};
  std::string node_name_;
};

}  // namespace waybar::modules
