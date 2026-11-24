#pragma once

#include <fmt/format.h>
#include <json/json.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "util/string.hpp"

namespace waybar::modules::hyprland {

inline bool hasPositionalZeroPlaceholder(std::string_view fmt_str) {
  for (std::size_t i = 0; i < fmt_str.size(); ++i) {
    if (fmt_str[i] == '{') {
      if (i + 1 < fmt_str.size() && fmt_str[i + 1] == '{') {
        ++i;
        continue;
      }
      if (i + 1 < fmt_str.size() && (fmt_str[i + 1] == '}' || fmt_str[i + 1] == ':')) {
        return true;
      }
      if (i + 2 < fmt_str.size() && fmt_str[i + 1] == '0' &&
          (fmt_str[i + 2] == '}' || fmt_str[i + 2] == ':')) {
        return true;
      }
    } else if (fmt_str[i] == '}' && i + 1 < fmt_str.size() && fmt_str[i + 1] == '}') {
      ++i;
    }
  }
  return false;
}

inline std::string formatLanguage(const std::string& full_name, const std::string& short_name,
                                  const std::string& variant, const std::string& short_description,
                                  const Json::Value& config, const std::string& base_format,
                                  const std::string& prefix) {
  const auto variant_key = prefix + "-" + short_description + "-" + variant;
  const auto lang_key = prefix + "-" + short_description;
  const Json::Value* override_val = nullptr;
  if (!variant.empty() && config[variant_key].isString()) {
    override_val = &config[variant_key];
  } else if (config[lang_key].isString()) {
    override_val = &config[lang_key];
  }

  if (override_val != nullptr) {
    const auto override_str = override_val->asString();
    std::string formatted_override = override_str;
    try {
      formatted_override = fmt::format(
          fmt::runtime(override_str), fmt::arg("long", full_name), fmt::arg("short", short_name),
          fmt::arg("shortDescription", short_description), fmt::arg("variant", variant));
    } catch (const fmt::format_error&) {
      // Keep literal override strings containing non-placeholder braces (e.g. "{Español}").
    }

    if (hasPositionalZeroPlaceholder(base_format)) {
      return trim(fmt::format(fmt::runtime(base_format), formatted_override,
                              fmt::arg("long", full_name), fmt::arg("short", short_name),
                              fmt::arg("shortDescription", short_description),
                              fmt::arg("variant", variant)));
    }
    return trim(formatted_override);
  }

  return trim(fmt::format(
      fmt::runtime(base_format), fmt::arg("long", full_name), fmt::arg("short", short_name),
      fmt::arg("shortDescription", short_description), fmt::arg("variant", variant)));
}

}  // namespace waybar::modules::hyprland
