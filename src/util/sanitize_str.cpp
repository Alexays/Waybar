#include <array>
#include <string>
#include <util/sanitize_str.hpp>
#include <utility>

namespace waybar::util {
// replaces ``<>&"'`` with their encoded counterparts
std::string sanitize_string(std::string str) {
  // note: it's important that '&' is replaced first; therefore we *can't* use std::map
  const std::pair<char, std::string> replacement_table[] = {
      {'&', "&amp;"}, {'<', "&lt;"}, {'>', "&gt;"}, {'"', "&quot;"}, {'\'', "&apos;"}};
  size_t startpoint;
  for (const auto& pair : replacement_table) {
    startpoint = 0;
    while ((startpoint = str.find(pair.first, startpoint)) != std::string::npos) {
      str.replace(startpoint, 1, pair.second);
      startpoint += pair.second.length();
    }
  }

  return str;
}
}  // namespace waybar::util
