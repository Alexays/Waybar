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
  static void updateSinkVolume(waybar::modules::Wireplumber* self);
  static void updateSourceVolume(waybar::modules::Wireplumber* self);
  static void updateSinkNodeName(waybar::modules::Wireplumber* self);
  static void updateSourceNodeName(waybar::modules::Wireplumber* self);
  static uint32_t getDefaultSinkNodeId(waybar::modules::Wireplumber* self);
  static uint32_t getDefaultSourceNodeId(waybar::modules::Wireplumber* self);
  static void onPluginActivated(WpObject* p, GAsyncResult* res, waybar::modules::Wireplumber* self);
  static void onObjectManagerInstalled(waybar::modules::Wireplumber* self);

  WpCore* wp_core_;
  GPtrArray* apis_;
  WpObjectManager* om_;
  uint32_t pending_plugins_;
  bool sinkmuted_;
  double sinkvolume_;
  uint32_t sinknode_id_{0};
  bool sourcemuted_;
  double sourcevolume_;
  uint32_t sourcenode_id_{0};
  std::string sinknode_name_;
  std::string sourcenode_name_;
};

}  // namespace waybar::modules
