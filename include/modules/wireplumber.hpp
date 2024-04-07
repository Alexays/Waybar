#pragma once

#include <fmt/format.h>
#include <wp/wp.h>

#include <algorithm>
#include <array>

#include "ALabel.hpp"

namespace wabar::modules {

class Wireplumber : public ALabel {
 public:
  Wireplumber(const std::string&, const Json::Value&);
  virtual ~Wireplumber();
  auto update() -> void override;

 private:
  void asyncLoadRequiredApiModules();
  void prepare();
  void activatePlugins();
  static void updateVolume(wabar::modules::Wireplumber* self, uint32_t id);
  static void updateNodeName(wabar::modules::Wireplumber* self, uint32_t id);
  static void onPluginActivated(WpObject* p, GAsyncResult* res, wabar::modules::Wireplumber* self);
  static void onDefaultNodesApiLoaded(WpObject* p, GAsyncResult* res,
                                      wabar::modules::Wireplumber* self);
  static void onMixerApiLoaded(WpObject* p, GAsyncResult* res, wabar::modules::Wireplumber* self);
  static void onObjectManagerInstalled(wabar::modules::Wireplumber* self);
  static void onMixerChanged(wabar::modules::Wireplumber* self, uint32_t id);
  static void onDefaultNodesApiChanged(wabar::modules::Wireplumber* self);

  bool handleScroll(GdkEventScroll* e) override;

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
};

}  // namespace wabar::modules
