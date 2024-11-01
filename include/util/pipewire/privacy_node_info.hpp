#pragma once

#include <gtkmm/icontheme.h>
#include <pipewire/pipewire.h>

namespace waybar::util::PipewireBackend {

enum PrivacyNodeType {
  PRIVACY_NODE_TYPE_NONE,
  PRIVACY_NODE_TYPE_VIDEO_INPUT,
  PRIVACY_NODE_TYPE_AUDIO_INPUT,
  PRIVACY_NODE_TYPE_AUDIO_OUTPUT
};

class PrivacyNodeInfo final {
 public:
  uint32_t id;
  PrivacyNodeType type = PRIVACY_NODE_TYPE_NONE;
  enum pw_node_state state = PW_NODE_STATE_IDLE;
  void *data;
  std::string media_class;
  struct spa_hook object_listener;
  struct spa_hook proxy_listener;

  std::string getName();
  std::string getIconName(const Glib::RefPtr<const Gtk::IconTheme> theme);
  // Handlers for PipeWire events
  void handleProxyEventDestroy();
  void handleNodeEventInfo(const struct pw_node_info *info);

 private:
  uint32_t client_id;
  std::string media_name;
  std::string node_name;
  std::string application_name;

  std::string pipewire_access_portal_app_id;
  std::string application_icon_name;
};

}  // namespace waybar::util::PipewireBackend
