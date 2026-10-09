#pragma once

#include <json/json.h>

#include <functional>
#include <regex>
#include <string>
#include <utility>

namespace waybar::util {

struct Rule {
  std::regex rule;
  std::string repr;
  int priority;

  // Fix for Clang < 16
  // See https://en.cppreference.com/w/cpp/compiler_support/20 "Parenthesized initialization of
  // aggregates"
  Rule(std::regex rule, std::string repr, int priority)
      : rule(std::move(rule)), repr(std::move(repr)), priority(priority) {}
};

int default_priority_function(std::string& key);

/* A collection of regexes and strings, with a default string to return if no regexes.
 * When a regex is matched, the corresponding string is returned.
 * All regexes that are matched are cached, so that the regexes are only
 * evaluated once against a given string.
 * Regexes may be given a higher priority than others, so that they are matched
 * first. The priority function is given the regex string, and should return a
 * higher number for higher priority regexes.
 */
class RegexCollection {
 private:
  std::vector<Rule> rules;
  std::map<std::string, std::string> regex_cache;
  std::string default_repr;

  std::string find_match(std::string& value, bool& matched_any);

 public:
  RegexCollection() = default;
  RegexCollection(
      const Json::Value& map, std::string default_repr = "",
      const std::function<int(std::string&)>& priority_function = default_priority_function);
  ~RegexCollection() = default;

  std::string& get(std::string& value, bool& matched_any);
  std::string& get(std::string& value);
};

}  // namespace waybar::util
