#pragma once

#include <fnmatch.h>
#include <json/json.h>
#include <spdlog/spdlog.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class IconManager {
 public:
  static IconManager& instance() {
    static IconManager instance;
    return instance;
  }

  // Keys are application IDs. A key containing '*' or '?' is a shell-style pattern (see
  // fnmatch(3)) matched against the whole ID; any other key must match it exactly, and an
  // exact key takes priority over a pattern.
  void setIconsConfig(const Json::Value& icons_config) {
    if (icons_config.isObject()) {
      for (const auto& key : icons_config.getMemberNames()) {
        std::string app_name = key;
        const Json::Value& icon_value = icons_config[key];

        if (icon_value.isBool() && !icon_value.asBool()) {
          // false value means hide this app
          if (isPattern(app_name)) {
            hidden_patterns_.push_back(app_name);
          } else {
            hidden_apps_.insert(app_name);
          }
        } else if (icon_value.isString()) {
          std::string icon_path = icon_value.asString();
          if (isPattern(app_name)) {
            icon_patterns_.emplace_back(app_name, icon_path);
          } else {
            icons_map_[app_name] = icon_path;
          }
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
    for (const auto& [pattern, icon] : icon_patterns_) {
      if (matches(pattern, app_name)) {
        return icon;
      }
    }
    return "";
  }

  bool isHidden(const std::string& app_name) const {
    if (hidden_apps_.find(app_name) != hidden_apps_.end()) {
      return true;
    }
    for (const auto& pattern : hidden_patterns_) {
      if (matches(pattern, app_name)) {
        return true;
      }
    }
    return false;
  }

 private:
  IconManager() = default;

  static bool isPattern(const std::string& key) {
    return key.find_first_of("*?") != std::string::npos;
  }

  static bool matches(const std::string& pattern, const std::string& app_name) {
    return fnmatch(pattern.c_str(), app_name.c_str(), 0) == 0;
  }

  std::unordered_map<std::string, std::string> icons_map_;
  std::unordered_set<std::string> hidden_apps_;
  // Wildcard keys; when several match an id, the first wins (jsoncpp lists keys alphabetically)
  std::vector<std::pair<std::string, std::string>> icon_patterns_;
  std::vector<std::string> hidden_patterns_;
};
