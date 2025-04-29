#pragma once

#include <json/json.h>
#include <spdlog/spdlog.h>

#include <string>
#include <unordered_map>

class IconManager {
 public:
  static IconManager& instance() {
    static IconManager instance;
    return instance;
  }

  void setIconsConfig(const Json::Value& icons_config) {
    if (icons_config.isObject()) {
      for (const auto& key : icons_config.getMemberNames()) {
        std::string app_name = key;
        const Json::Value& icon_value = icons_config[key];

        if (icon_value.isString()) {
          std::string icon_path = icon_value.asString();
          icons_map_[app_name] = icon_path;
        }
      }
    } else {
      spdlog::warn("Invalid icon config format.");
    }
  }

  std::string getIconForApp(const std::string& app_name) const {
    auto it = icons_map_.find(app_name);
    if (it != icons_map_.end()) {
      return it->second;
    }
    return "";
  }

 private:
  IconManager() = default;
  std::unordered_map<std::string, std::string> icons_map_;
};
