#pragma once

namespace waybar::util {

const static auto NOOP = []() {};
enum class ChangeType : char { Increase, Decrease };

}  // namespace waybar::util
