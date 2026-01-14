#include "config.hpp"

#include <spdlog/spdlog.h>
#include <unistd.h>
#ifndef __OpenBSD__
#include <wordexp.h>
#else
#include <glob.h>
#endif

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

std::vector<std::string> Config::tryExpandPath(const std::string &base,
                                               const std::string &filename) {
  fs::path path;

  if (!filename.empty()) {
    path = fs::path(base) / fs::path(filename);
  } else {
    path = fs::path(base);
  }

  spdlog::debug("Try expanding: {}", path.string());

  std::vector<std::string> results;
#ifndef __OpenBSD__
  wordexp_t p;
  if (wordexp(path.c_str(), &p, 0) == 0) {
    for (size_t i = 0; i < p.we_wordc; i++) {
      if (access(p.we_wordv[i], F_OK) == 0) {
        results.emplace_back(p.we_wordv[i]);
        spdlog::debug("Found config file: {}", p.we_wordv[i]);
      }
    }
    wordfree(&p);
  }
#else
  glob_t p;
  if (glob(path.c_str(), 0, NULL, &p) == 0) {
    for (size_t i = 0; i < p.gl_pathc; i++) {
      if (access(p.gl_pathv[i], F_OK) == 0) {
        results.emplace_back(p.gl_pathv[i]);
        spdlog::debug("Found config file: {}", p.gl_pathv[i]);
      }
    }
    globfree(&p);
  }
#endif

  return results;
}

std::optional<std::string> Config::findConfigPath(const std::vector<std::string> &names,
                                                  const std::vector<std::string> &dirs) {
  if (const char *dir = std::getenv(Config::CONFIG_PATH_ENV)) {
    for (const auto &name : names) {
      if (auto res = tryExpandPath(dir, name); !res.empty()) {
        return res.front();
      }
    }
  }

  for (const auto &dir : dirs) {
    for (const auto &name : names) {
      if (auto res = tryExpandPath(dir, name); !res.empty()) {
        return res.front();
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

std::vector<std::string> Config::findIncludePath(const std::string &name,
                                                 const std::vector<std::string> &dirs) {
  auto match1 = tryExpandPath(name, "");
  if (!match1.empty()) {
    return match1;
  }
  if (const char *dir = std::getenv(Config::CONFIG_PATH_ENV)) {
    if (auto res = tryExpandPath(dir, name); !res.empty()) {
      return res;
    }
  }
  for (const auto &dir : dirs) {
    if (auto res = tryExpandPath(dir, name); !res.empty()) {
      return res;
    }
  }

  return {};
}

void Config::resolveConfigIncludes(Json::Value &config, int depth) {
  Json::Value includes = config["include"];
  if (includes.isArray()) {
    for (const auto &include : includes) {
      spdlog::info("Including resource file: {}", include.asString());
      auto matches = findIncludePath(include.asString());
      if (!matches.empty()) {
        for (const auto &match : matches) {
          setupConfig(config, match, depth + 1);
        }
      } else {
        spdlog::warn("Unable to find resource file: {}", include.asString());
      }
    }
  } else if (includes.isString()) {
    spdlog::info("Including resource file: {}", includes.asString());
    auto matches = findIncludePath(includes.asString());
    if (!matches.empty()) {
      for (const auto &match : matches) {
        setupConfig(config, match, depth + 1);
      }
    } else {
      spdlog::warn("Unable to find resource file: {}", includes.asString());
    }
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
  const auto isOutputMatches = [&](const std::string &output) -> bool {
    if (output.substr(0, 1) == "$") {
      auto *const environment_value = std::getenv(output.substr(1).c_str());
      if (environment_value != nullptr) {
        const std::string output_from_env = environment_value;
        return output_from_env == name || output_from_env == identifier;
      }

      spdlog::warn("The environment value is unknown: {}", output);
    }

    return output == name || output == identifier;
  };

  if (config["output"].isArray()) {
    for (auto const &output_conf : config["output"]) {
      if (output_conf.isString()) {
        auto config_output = output_conf.asString();

        if (config_output.substr(0, 1) == "!") {
          if (isOutputMatches(config_output.substr(1))) {
            return false;
          }

          continue;
        }

        if (isOutputMatches(config_output)) {
          return true;
        }

        if (config_output.substr(0, 1) == "*") {
          return true;
        }
      }
    }
    return false;
  }

  if (config["output"].isString()) {
    auto config_output = config["output"].asString();

    if (!config_output.empty()) {
      if (config_output.substr(0, 1) == "!") {
        return !isOutputMatches(config_output.substr(1));
      }

      return isOutputMatches(config_output);
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
  config_ = Json::Value();
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
