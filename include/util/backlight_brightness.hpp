#pragma once

#include <algorithm>
#include <cmath>

namespace waybar::util {

template <typename Write>
void scroll_brightness(int current, int maximum, bool increase, double step, double minimum,
                       Write&& write) {
  if (maximum <= 0 || !std::isfinite(step) || step <= 0 || !std::isfinite(minimum)) {
    return;
  }

  current = std::clamp(current, 0, maximum);
  step = std::min(step, 100.0);
  int delta = std::max(1, static_cast<int>(step * maximum / 100.0));
  // Compare in percent to avoid moving a rounding boundary through conversion.
  if ((delta + 0.5) * 100.0 / maximum <= step) {
    ++delta;
  }
  if (increase) {
    // Equality with the cache does not imply equality with the hardware.
    write(current + std::min(delta, maximum - current));
    return;
  }

  minimum = std::clamp(minimum, 0.0, 100.0);
  int lower = static_cast<int>(minimum * maximum / 100.0);
  if (lower * 100.0 / maximum < minimum) {
    ++lower;
  }
  if (current > lower) {
    write(current - std::min(delta, current - lower));
  }
}

}  // namespace waybar::util
