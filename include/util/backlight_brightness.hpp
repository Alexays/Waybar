#pragma once

#include <algorithm>
#include <cmath>

namespace waybar::util {

// Calculate in device units; rounded display percentages must not decide whether
// the hardware has reached a limit. Invalid configuration leaves it unchanged.
inline int brightness_after_scroll(int current, int maximum, bool increase, double step,
                                   double minimum) {
  if (maximum <= 0 || !std::isfinite(step) || step <= 0 || !std::isfinite(minimum)) {
    return current;
  }

  current = std::clamp(current, 0, maximum);
  // Bound percentages before multiplication and conversion to int. A positive
  // step must move at least one unit, including on devices with small ranges.
  const int delta =
      std::max(1, static_cast<int>(std::round(std::min(step, 100.0) * maximum / 100.0)));
  if (increase) {
    return current + std::min(delta, maximum - current);
  }

  // Round up so a fractional minimum is respected at the device's resolution.
  const int lower =
      minimum <= 0
          ? 0
          : std::max(1, static_cast<int>(std::ceil(std::min(minimum, 100.0) * maximum / 100.0)));
  // Do not raise brightness when scrolling down from below the configured limit.
  return current - std::min(delta, std::max(0, current - lower));
}

}  // namespace waybar::util
