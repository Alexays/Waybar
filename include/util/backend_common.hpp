#pragma once

#include "AModule.hpp"

namespace wabar::util {

const static auto NOOP = []() {};
enum class ChangeType : char { Increase, Decrease };

}  // namespace wabar::util