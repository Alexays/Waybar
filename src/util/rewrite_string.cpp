#include "util/rewrite_string.hpp"

#include <spdlog/spdlog.h>

#include <iterator>
#include <regex>
#include <utility>

#include "util/regex_input.hpp"

namespace waybar::util {
std::string rewriteString(const std::string& value, const Json::Value& rules) {
  if (!rules.isObject()) {
    return value;
  }

  std::string res = value;

  for (auto it = rules.begin(); it != rules.end(); ++it) {
    if (it.key().isString() && it->isString()) {
      try {
        // malformated regexes will cause an exception.
        // in this case, log error and try the next rule.
        const std::regex rule{it.key().asString(), std::regex_constants::icase};
        RegexInput input{res};
        if (std::regex_match(input.begin(), input.end(), rule)) {
          std::string rewritten;
          std::regex_replace(std::back_inserter(rewritten), input.begin(), input.end(), rule,
                             it->asString());
          res = std::move(rewritten);
        }
      } catch (const std::regex_error& e) {
        if (e.code() == std::regex_constants::error_complexity) {
          spdlog::warn("Rule {} skipped: matching takes too long", it.key().asString());
        } else {
          spdlog::error("Invalid rule {}: {}", it.key().asString(), e.what());
        }
      }
    }
  }

  return res;
}
}  // namespace waybar::util
