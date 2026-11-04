#include "util/regex_collection.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <utility>

#include "util/regex_input.hpp"

namespace waybar::util {

int default_priority_function(std::string& key) { return 0; }

// match.format(repr), with the braces doubled in what each $ reference expands to. Each reference
// is cut out as std::match_results::format parses it and expanded by format() itself.
static std::string formatEscaped(const RegexInput::Match& match, const std::string& repr) {
  std::string res;
  std::string expanded;
  size_t literal = 0;
  for (size_t pos = repr.find('$'); pos != std::string::npos; pos = repr.find('$', literal)) {
    res.append(repr, literal, pos - literal);
    size_t len = 1;
    if (pos + 1 < repr.size()) {
      const char c = repr[pos + 1];
      if (c == '$' || c == '&' || c == '`' || c == '\'') {
        len = 2;
      } else if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
        len = pos + 2 < repr.size() && std::isdigit(static_cast<unsigned char>(repr[pos + 2])) != 0
                  ? 3
                  : 2;
      }
    }
    expanded.clear();
    match.format(std::back_inserter(expanded), repr.data() + pos, repr.data() + pos + len);
    for (const char c : expanded) {
      if (c == '{' || c == '}') {
        res.push_back(c);
      }
      res.push_back(c);
    }
    literal = pos + len;
  }
  res.append(repr, literal);
  return res;
}

RegexCollection::RegexCollection(const Json::Value& map, std::string default_repr,
                                 const std::function<int(std::string&)>& priority_function,
                                 bool escape_captures)
    : default_repr(std::move(default_repr)), escape_captures(escape_captures) {
  if (!map.isObject()) {
    spdlog::warn("Mapping is not an object");
    return;
  }

  for (auto it = map.begin(); it != map.end(); ++it) {
    if (it.key().isString() && it->isString()) {
      std::string key = it.key().asString();
      int priority = priority_function(key);
      try {
        const std::regex rule{key, std::regex_constants::icase};
        rules.emplace_back(rule, it->asString(), priority);
      } catch (const std::regex_error& e) {
        spdlog::error("Invalid rule '{}': {}", key, e.what());
      }
    }
  }

  std::sort(rules.begin(), rules.end(), [](Rule& a, Rule& b) { return a.priority > b.priority; });
}

std::string RegexCollection::find_match(std::string& value, bool& matched_any) {
  for (auto& rule : rules) {
    try {
      RegexInput input{value};
      RegexInput::Match match;
      if (std::regex_search(input.begin(), input.end(), match, rule.rule)) {
        std::string repr =
            escape_captures ? formatEscaped(match, rule.repr) : match.format(rule.repr);
        matched_any = true;
        return repr;
      }
    } catch (const std::regex_error&) {
      spdlog::warn("Rule for {} skipped: matching takes too long", rule.repr);
    }
  }

  return value;
}

std::string& RegexCollection::get(std::string& value, bool& matched_any) {
  if (auto it = regex_cache.find(value); it != regex_cache.end()) {
    return it->second;
  }

  std::string repr = find_match(value, matched_any);

  if (!matched_any) {
    repr = default_repr;
  }

  // Keys carry window titles, which clients change at will
  if (regex_cache.size() >= MAX_CACHE_ENTRIES) {
    regex_cache.clear();
  }

  return regex_cache.emplace(value, std::move(repr)).first->second;
}

std::string& RegexCollection::get(std::string& value) {
  bool matched_any = false;
  return get(value, matched_any);
}

}  // namespace waybar::util
