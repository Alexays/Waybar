#pragma once

#include <pipewire/pipewire.h>

#include <string>

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

  struct spa_hook node_listener;

  bool changed = false;

  void* data;

  PrivacyNodeInfo(uint32_t id_, void* data_) : id(id_), data(data_) {}

  ~PrivacyNodeInfo() { spa_hook_remove(&node_listener); }
};
}  // namespace waybar::util::PipewireBackend
