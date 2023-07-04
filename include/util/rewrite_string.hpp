#pragma once
#include <json/json.h>

#include <string>

namespace waybar::util {
std::string rewriteString(const std::string&, const Json::Value&);
}
