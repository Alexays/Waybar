#pragma once

#include <optional>
#include <regex>
#include <string>

namespace waybar::util {

/**
 * Extract a value from slider tool output, clamped to [min, max]; nullopt on
 * failure so a slider can mark the read stale rather than synthesise 0. If regex
 * is set, capture group 1 is the value, otherwise the first line has a trailing
 * '%' stripped and is std::stod'd. nan/inf and unparseable input yield nullopt.
 */
std::optional<double> parseSliderValue(const std::string& raw, int min, int max,
                                       const std::optional<std::regex>& regex);

}  // namespace waybar::util
