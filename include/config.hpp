#pragma once

#include <json/json.h>

#include <string>

namespace waybar {

class Config {
 public:
  Config() = default;

  void load(const std::string &config, const std::string &style);

  const std::string &getStyle() { return css_file_; }

  Json::Value &getConfig() { return config_; }

  std::vector<Json::Value> getOutputConfigs(const std::string &name, const std::string &identifier);

 private:
  void setupConfig(const std::string &config_file, int depth);
  void resolveConfigIncludes(Json::Value &config, int depth);
  void mergeConfig(Json::Value &a_config_, Json::Value &b_config_);

  std::string config_file_;
  std::string css_file_;

  Json::Value config_;
};
}  // namespace waybar
