#include "util/rewrite_title.hpp"

#include <spdlog/spdlog.h>

#include <regex>

namespace waybar::util {
std::string rewriteTitle(const std::string& title, const Json::Value& rules) {
  if (!rules.isObject()) {
    return title;
  }

  std::string res = title;

  for (auto it = rules.begin(); it != rules.end(); ++it) {
    if (it.key().isString() && it->isString()) {
      try {
        // malformated regexes will cause an exception.
        // in this case, log error and try the next rule.
        const std::regex rule{it.key().asString()};
        if (std::regex_match(title, rule)) {
          res = std::regex_replace(res, rule, it->asString());
        }
      } catch (const std::regex_error& e) {
        spdlog::error("Invalid rule {}: {}", it.key().asString(), e.what());
      }
    }
  }

  return res;
}
}  // namespace waybar::util
