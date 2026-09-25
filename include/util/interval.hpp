#pragma once

#include <json/json.h>

#include <chrono>
#include <cstdint>

namespace waybar::util {

/**
 * Parse a module's "interval" config into milliseconds, the shared convention
 * across the label/graph/slider bases. "once" yields max(); a positive number is
 * seconds, floored at 1 ms; an absent key falls back to default_seconds. A numeric
 * 0 (or negative) keeps the event-driven 0 sentinel only when default_seconds is 0,
 * otherwise it falls back to the default so a periodic module cannot busy-loop or
 * hit modulo-by-zero clock code. Fixes #4521.
 */
std::chrono::milliseconds parseInterval(const Json::Value& config, uint16_t default_seconds);

}  // namespace waybar::util
