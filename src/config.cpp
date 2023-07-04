#include "config.hpp"

#include <spdlog/spdlog.h>
#include <unistd.h>
#include <wordexp.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "util/json.hpp"

namespace fs = std::filesystem;

namespace waybar {

const std::vector<std::string> Config::CONFIG_DIRS = {
    "$XDG_CONFIG_HOME/waybar/", "$HOME/.config/waybar/",   "$HOME/waybar/",
    "/etc/xdg/waybar/",         SYSCONFDIR "/xdg/waybar/", "./resources/",
};

const char *Config::CONFIG_PATH_ENV = "WAYBAR_CONFIG_DIR";

std::optional<std::string> tryExpandPath(const std::string base, const std::string filename) {
  fs::path path;

  if (filename != "") {
    path = fs::path(base) / fs::path(filename);
  } else {
    path = fs::path(base);
  }

  spdlog::debug("Try expanding: {}", path.string());

  wordexp_t p;
  if (wordexp(path.c_str(), &p, 0) == 0) {
    if (access(*p.we_wordv, F_OK) == 0) {
      std::string result = *p.we_wordv;
      wordfree(&p);
      spdlog::debug("Found config file: {}", path.string());
      return result;
    }
    wordfree(&p);
  }
  return std::nullopt;
}

std::optional<std::string> Config::findConfigPath(const std::vector<std::string> &names,
                                                  const std::vector<std::string> &dirs) {
  if (const char *dir = std::getenv(Config::CONFIG_PATH_ENV)) {
    for (const auto &name : names) {
      if (auto res = tryExpandPath(dir, name); res) {
        return res;
      }
    }
  }

  for (const auto &dir : dirs) {
    for (const auto &name : names) {
      if (auto res = tryExpandPath(dir, name); res) {
        return res;
      }
    }
  }
  return std::nullopt;
}

void Config::setupConfig(Json::Value &dst, const std::string &config_file, int depth) {
  if (depth > 100) {
    throw std::runtime_error("Aborting due to likely recursive include in config files");
  }
  std::ifstream file(config_file);
  if (!file.is_open()) {
    throw std::runtime_error("Can't open config file");
  }
  std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  util::JsonParser parser;
  Json::Value tmp_config = parser.parse(str);
  if (tmp_config.isArray()) {
    for (auto &config_part : tmp_config) {
      resolveConfigIncludes(config_part, depth);
    }
  } else {
    resolveConfigIncludes(tmp_config, depth);
  }
  mergeConfig(dst, tmp_config);
}

void Config::resolveConfigIncludes(Json::Value &config, int depth) {
  Json::Value includes = config["include"];
  if (includes.isArray()) {
    for (const auto &include : includes) {
      spdlog::info("Including resource file: {}", include.asString());
      setupConfig(config, tryExpandPath(include.asString(), "").value_or(""), ++depth);
    }
  } else if (includes.isString()) {
    spdlog::info("Including resource file: {}", includes.asString());
    setupConfig(config, tryExpandPath(includes.asString(), "").value_or(""), ++depth);
  }
}

void Config::mergeConfig(Json::Value &a_config_, Json::Value &b_config_) {
  if (!a_config_) {
    // For the first config
    a_config_ = b_config_;
  } else if (a_config_.isObject() && b_config_.isObject()) {
    for (const auto &key : b_config_.getMemberNames()) {
      // [] creates key with default value. Use `get` to avoid that.
      if (a_config_.get(key, Json::Value::nullSingleton()).isObject() &&
          b_config_[key].isObject()) {
        mergeConfig(a_config_[key], b_config_[key]);
      } else if (!a_config_.isMember(key)) {
        // do not allow overriding value set by top or previously included config
        a_config_[key] = b_config_[key];
      } else {
        spdlog::trace("Option {} is already set; ignoring value {}", key, b_config_[key]);
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
      if (output_conf.isString()) {
        auto config_output = output_conf.asString();
        if (config_output.substr(0, 1) == "!") {
          if (config_output.substr(1) == name || config_output.substr(1) == identifier) {
            return false;
          } else {
            continue;
          }
        }
        if (config_output == name || config_output == identifier) {
          return true;
        }
        if (config_output.substr(0, 1) == "*") {
          return true;
        }
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
  setupConfig(config_, config_file_, 0);
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
