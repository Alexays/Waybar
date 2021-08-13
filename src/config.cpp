#include "config.hpp"

#include <spdlog/spdlog.h>
#include <unistd.h>
#include <wordexp.h>

#include <fstream>
#include <stdexcept>

#include "util/json.hpp"

namespace waybar {

const std::vector<std::string> Config::CONFIG_DIRS = {
    "$XDG_CONFIG_HOME/waybar/",
    "$HOME/.config/waybar/",
    "$HOME/waybar/",
    "/etc/xdg/waybar/",
    SYSCONFDIR "/xdg/waybar/",
    "./resources/",
};

std::optional<std::string> tryExpandPath(const std::string &path) {
  wordexp_t p;
  if (wordexp(path.c_str(), &p, 0) == 0) {
    if (access(*p.we_wordv, F_OK) == 0) {
      std::string result = *p.we_wordv;
      wordfree(&p);
      return result;
    }
    wordfree(&p);
  }
  return std::nullopt;
}

std::optional<std::string> Config::findConfigPath(const std::vector<std::string> &names,
                                                  const std::vector<std::string> &dirs) {
  std::vector<std::string> paths;
  for (const auto &dir : dirs) {
    for (const auto &name : names) {
      if (auto res = tryExpandPath(dir + name); res) {
        return res;
      }
    }
  }
  return std::nullopt;
}

void Config::setupConfig(const std::string &config_file, int depth) {
  if (depth > 100) {
    throw std::runtime_error("Aborting due to likely recursive include in config files");
  }
  std::ifstream file(config_file);
  if (!file.is_open()) {
    throw std::runtime_error("Can't open config file");
  }
  std::string      str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  util::JsonParser parser;
  Json::Value      tmp_config = parser.parse(str);
  if (tmp_config.isArray()) {
    for (auto &config_part : tmp_config) {
      resolveConfigIncludes(config_part, depth);
    }
  } else {
    resolveConfigIncludes(tmp_config, depth);
  }
  mergeConfig(config_, tmp_config);
}

void Config::resolveConfigIncludes(Json::Value &config, int depth) {
  Json::Value includes = config["include"];
  if (includes.isArray()) {
    for (const auto &include : includes) {
      spdlog::info("Including resource file: {}", include.asString());
      setupConfig(tryExpandPath(include.asString()).value_or(""), ++depth);
    }
  } else if (includes.isString()) {
    spdlog::info("Including resource file: {}", includes.asString());
    setupConfig(tryExpandPath(includes.asString()).value_or(""), ++depth);
  }
}

void Config::mergeConfig(Json::Value &a_config_, Json::Value &b_config_) {
  if (!a_config_) {
    // For the first config
    a_config_ = b_config_;
  } else if (a_config_.isObject() && b_config_.isObject()) {
    for (const auto &key : b_config_.getMemberNames()) {
      if (a_config_[key].isObject() && b_config_[key].isObject()) {
        mergeConfig(a_config_[key], b_config_[key]);
      } else {
        a_config_[key] = b_config_[key];
      }
    }
  } else if (a_config_.isArray() && b_config_.isArray()) {
    // This can happen only on the top-level array of a multi-bar config
    for (Json::Value::ArrayIndex i = 0; i < b_config_.size(); i++) {
      if (a_config_[i].isObject() && b_config_[i].isObject()) {
        mergeConfig(a_config_[i], b_config_[i]);
      }
    }
  } else {
    spdlog::error("Cannot merge config, conflicting or invalid JSON types");
  }
}
bool isValidOutput(const Json::Value &config, const std::string &name,
                   const std::string &identifier) {
  if (config["output"].isArray()) {
    for (auto const &output_conf : config["output"]) {
      if (output_conf.isString() &&
          (output_conf.asString() == name || output_conf.asString() == identifier)) {
        return true;
      }
    }
    return false;
  } else if (config["output"].isString()) {
    auto config_output = config["output"].asString();
    if (!config_output.empty()) {
      if (config_output.substr(0, 1) == "!") {
        return config_output.substr(1) != name && config_output.substr(1) != identifier;
      }
      return config_output == name || config_output == identifier;
    }
  }

  return true;
}

void Config::load(const std::string &config) {
  auto file = config.empty() ? findConfigPath({"config", "config.jsonc"}) : config;
  if (!file) {
    throw std::runtime_error("Missing required resource files");
  }
  config_file_ = file.value();
  spdlog::info("Using configuration file {}", config_file_);
  setupConfig(config_file_, 0);
}

std::vector<Json::Value> Config::getOutputConfigs(const std::string &name,
                                                  const std::string &identifier) {
  std::vector<Json::Value> configs;
  if (config_.isArray()) {
    for (auto const &config : config_) {
      if (config.isObject() && isValidOutput(config, name, identifier)) {
        configs.push_back(config);
      }
    }
  } else if (isValidOutput(config_, name, identifier)) {
    configs.push_back(config_);
  }
  return configs;
}

}  // namespace waybar
