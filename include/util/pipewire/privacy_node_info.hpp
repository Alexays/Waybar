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

  std::string pipewire_access_portal_app_id;
  std::string application_icon_name;

  struct spa_hook node_listener;

  bool changed = false;

  void *data;

  PrivacyNodeInfo(uint32_t id_, void *data_) : id(id_), data(data_) {}

  ~PrivacyNodeInfo() { spa_hook_remove(&node_listener); }

  std::string get_name() {
    const std::vector<std::string *> names{&application_name, &node_name};
    std::string name = "Unknown Application";
    for (auto &name_ : names) {
      if (name_ != nullptr && name_->length() > 0) {
        name = *name_;
        name[0] = toupper(name[0]);
        break;
      }
    }
    return name;
  }

  std::string get_icon_name() {
    const std::vector<std::string *> names{&application_icon_name, &pipewire_access_portal_app_id,
                                           &application_name, &node_name};
    std::string name = "application-x-executable-symbolic";
    for (auto &name_ : names) {
      if (name_ != nullptr && name_->length() > 0 && DefaultGtkIconThemeWrapper::has_icon(*name_)) {
        return *name_;
      }
    }
    return name;
  }
};
}  // namespace waybar::util::PipewireBackend
