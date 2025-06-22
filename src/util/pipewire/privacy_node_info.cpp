#include "util/pipewire/privacy_node_info.hpp"

namespace waybar::util::PipewireBackend {

std::string PrivacyNodeInfo::getName() {
  const std::vector<std::string *> names{&application_name, &node_name};
  std::string name = "Unknown Application";
  for (const auto &item : names) {
    if (item != nullptr && !item->empty()) {
      name = *item;
      name[0] = toupper(name[0]);
      break;
    }
  }
  return name;
}

std::string PrivacyNodeInfo::getIconName() {
  const std::vector<std::string *> names{&application_icon_name, &pipewire_access_portal_app_id,
                                         &application_name, &node_name};
  std::string name = "application-x-executable-symbolic";
  for (const auto &item : names) {
    if (item != nullptr && !item->empty() && DefaultGtkIconThemeWrapper::has_icon(*item)) {
      return *item;
    }
  }
  return name;
}

void PrivacyNodeInfo::handleProxyEventDestroy() {
  spa_hook_remove(&proxy_listener);
  spa_hook_remove(&object_listener);
}

void PrivacyNodeInfo::handleNodeEventInfo(const struct pw_node_info *info) {
  state = info->state;

  const struct spa_dict_item *item;
  spa_dict_for_each(item, info->props) {
    if (strcmp(item->key, PW_KEY_CLIENT_ID) == 0) {
      client_id = strtoul(item->value, nullptr, 10);
    } else if (strcmp(item->key, PW_KEY_MEDIA_NAME) == 0) {
      media_name = item->value;
    } else if (strcmp(item->key, PW_KEY_NODE_NAME) == 0) {
      node_name = item->value;
    } else if (strcmp(item->key, PW_KEY_APP_NAME) == 0) {
      application_name = item->value;
    } else if (strcmp(item->key, "pipewire.access.portal.app_id") == 0) {
      pipewire_access_portal_app_id = item->value;
    } else if (strcmp(item->key, PW_KEY_APP_ICON_NAME) == 0) {
      application_icon_name = item->value;
    } else if (strcmp(item->key, "stream.monitor") == 0) {
      is_monitor = strcmp(item->value, "true") == 0;
    }
  }
}

}  // namespace waybar::util::PipewireBackend
