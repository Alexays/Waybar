#pragma once

#include <json/value.h>

namespace waybar::util {
bool valid_host(const Json::Value& config);
}  // namespace waybar::util
