#include "util/slider_value.hpp"

#include <algorithm>
#include <cmath>

namespace waybar::util {

std::optional<double> parseSliderValue(const std::string& raw, int min, int max,
                                       const std::optional<std::regex>& regex) {
  std::string s = raw.substr(0, raw.find('\n'));  // first line only
  if (regex) {
    // A configured regex that matches nothing means the output is unexpected:
    // stay stale rather than guess with the heuristic.
    std::smatch match;
    if (!std::regex_search(s, match, *regex) || match.size() < 2) {
      return std::nullopt;
    }
    s = match[1].str();
  }
  auto begin = s.find_first_not_of(" \t\r");
  if (begin == std::string::npos) {
    return std::nullopt;
  }
  s = s.substr(begin, s.find_last_not_of(" \t\r") - begin + 1);
  if (s.back() == '%') {
    s.pop_back();
  }
  try {
    double value = std::stod(s);
    if (!std::isfinite(value)) {  // reject nan/inf
      return std::nullopt;
    }
    return std::clamp(value, static_cast<double>(min), static_cast<double>(max));
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace waybar::util
