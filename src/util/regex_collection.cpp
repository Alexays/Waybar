#include "util/regex_collection.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <utility>

namespace waybar::util {

int default_priority_function(std::string& key) { return 0; }

RegexCollection::RegexCollection(const Json::Value& map, std::string default_repr,
                                 const std::function<int(std::string&)>& priority_function)
    : default_repr(std::move(default_repr)) {
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
    std::smatch match;
    if (std::regex_search(value, match, rule.rule)) {
      matched_any = true;
      return match.format(rule.repr.data());
    }
  }

  return value;
}

std::string& RegexCollection::get(std::string& value, bool& matched_any) {
  if (regex_cache.contains(value)) {
    return regex_cache[value];
  }

  // std::string repr =
  // waybar::util::find_match(value, window_rewrite_rules_, matched_any);

  std::string repr = find_match(value, matched_any);

  if (!matched_any) {
    repr = default_repr;
  }

  regex_cache.emplace(value, repr);

  return regex_cache[value];  // Necessary in order to return a reference to the heap
}

std::string& RegexCollection::get(std::string& value) {
  bool matched_any = false;
  return get(value, matched_any);
}

}  // namespace waybar::util
