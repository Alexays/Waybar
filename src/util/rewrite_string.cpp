#include "util/rewrite_string.hpp"

#include <spdlog/spdlog.h>

#include <regex>

namespace waybar::util {
std::string rewriteString(const std::string& value, const Json::Value& rules) {
  if (rules.isObject()) {
    // TODO: convert objects to the array structure to accept existing configs.
  }

  if (!rules.isArray()) {
    return value;
  }

  std::string res = value;

  for (Json::Value::ArrayIndex i = 0; i != rules.size(); i++) {
    if (rules[i].isArray() && rules[i][0].isString() && rules[i][1].isString()) {
      try {
        // malformed regexes will cause an exception.
        // in this case, log error and try the next rule.
        const std::regex rule{rules[i][0].asString()};
        if (std::regex_match(res, rule)) {
          res = std::regex_replace(res, rule, rules[i][1].asString());
        }
      } catch (const std::regex_error& e) {
        spdlog::error("Invalid rule {}: {}", rules[i][0].asString(), e.what());
      }
    }
  }

  return res;
}
}  // namespace waybar::util
