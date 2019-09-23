#pragma once

#include <sstream>

namespace waybar::util {

std::string pow_format(unsigned long long value, const std::string &unit, bool binary = false);

}
