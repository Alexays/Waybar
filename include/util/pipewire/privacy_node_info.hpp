#pragma once

#include <pipewire/pipewire.h>

#include <string>

#include "util/gtk_icon.hpp"

namespace waybar::util::PipewireBackend {

enum PrivacyNodeType {
  PRIVACY_NODE_TYPE_NONE,
  PRIVACY_NODE_TYPE_VIDEO_INPUT,
  PRIVACY_NODE_TYPE_AUDIO_INPUT,
  PRIVACY_NODE_TYPE_AUDIO_OUTPUT
};

class PrivacyNodeInfo {
 public:
  PrivacyNodeType type = PRIVACY_NODE_TYPE_NONE;
  uint32_t id;
  uint32_t client_id;
  enum pw_node_state state = PW_NODE_STATE_IDLE;
  std::string media_class;
  std::string media_name;
  std::string node_name;
  std::string application_name;
  bool is_monitor = false;

  std::string pipewire_access_portal_app_id;
  std::string application_icon_name;

  struct spa_hook object_listener;
  struct spa_hook proxy_listener;

  void *data;

  std::string getName();
  std::string getIconName();

  // Handlers for PipeWire events
  void handleProxyEventDestroy();
  void handleNodeEventInfo(const struct pw_node_info *info);
};

}  // namespace waybar::util::PipewireBackend
