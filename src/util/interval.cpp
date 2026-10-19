#include "util/interval.hpp"

#include <algorithm>

namespace waybar::util {

std::chrono::milliseconds parseInterval(const Json::Value& config, uint16_t default_seconds) {
  const auto& interval = config["interval"];
  if (interval == "once") {
    return std::chrono::milliseconds::max();
  }
  if (interval.isNumeric()) {
    if (interval.asDouble() > 0) {
      return std::chrono::milliseconds(std::max(1L, static_cast<long>(interval.asDouble() * 1000)));
    }
    // A numeric 0/negative is an event-driven sentinel only where the default is
    // already 0; a periodic module falls back to its default instead of busy-looping.
    return std::chrono::milliseconds(default_seconds == 0 ? 0L : 1000L * default_seconds);
  }
  return std::chrono::milliseconds(1000L * default_seconds);
}

}  // namespace waybar::util
