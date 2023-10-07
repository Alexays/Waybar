#pragma once
#include <json/json.h>

#include <string>

namespace waybar::util {
std::string rewriteString(const std::string&, const Json::Value&);
std::string rewriteStringOnce(const std::string& value, const Json::Value& rules,
                              bool& matched_any);
}  // namespace waybar::util
