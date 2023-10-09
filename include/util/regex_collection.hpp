#pragma once

#include <json/json.h>

#include <functional>
#include <regex>
#include <string>

namespace waybar::util {

struct Rule {
  std::regex rule;
  std::string repr;
  int priority;
};

int default_priority_function(std::string& key);

class RegexCollection {
 private:
  std::vector<Rule> rules;
  std::map<std::string, std::string> regex_cache;
  std::string default_repr;

  std::string& find_match(std::string& value, bool& matched_any);

 public:
  RegexCollection() = default;
  RegexCollection(const Json::Value& map, std::string default_repr = "",
                  std::function<int(std::string&)> priority_function = default_priority_function);
  ~RegexCollection() = default;

  std::string& get(std::string& value, bool& matched_any);
  std::string& get(std::string& value);
};

}  // namespace waybar::util