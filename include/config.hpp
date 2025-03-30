#pragma once

#include <json/json.h>

#include <optional>
#include <string>

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

namespace waybar {

class Config {
 public:
  static const std::vector<std::string> CONFIG_DIRS;
  static const char *CONFIG_PATH_ENV;

  /* Try to find any of provided names in the supported set of config directories */
  static std::optional<std::string> findConfigPath(
      const std::vector<std::string> &names, const std::vector<std::string> &dirs = CONFIG_DIRS);

  static std::vector<std::string> tryExpandPath(const std::string &base,
                                                const std::string &filename);

  Config() = default;

  void load(const std::string &config);

  Json::Value &getConfig() { return config_; }

  std::vector<Json::Value> getOutputConfigs(const std::string &name, const std::string &identifier);

 private:
  void setupConfig(Json::Value &dst, const std::string &config_file, int depth);
  void resolveConfigIncludes(Json::Value &config, int depth);
  void mergeConfig(Json::Value &a_config_, Json::Value &b_config_);

  std::string config_file_;

  Json::Value config_;
};
}  // namespace waybar
