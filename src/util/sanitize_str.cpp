#include <array>
#include <string>
#include <util/sanitize_str.hpp>
#include <utility>

namespace waybar::util {
// replaces ``<>&"'`` with their encoded counterparts
std::string sanitize_string(std::string str) {
  // note: it's important that '&' is replaced first; therefor we *can't* use std::map
  const std::pair<char, std::string> replacement_table[] = {
      {'&', "&amp;"}, {'<', "&lt;"}, {'>', "&gt;"}, {'"', "&quot;"}, {'\'', "&apos;"}};
  size_t startpoint;
  for (size_t i = 0; i < (sizeof(replacement_table) / sizeof(replacement_table[0])); ++i) {
    startpoint = 0;
    std::pair pair = replacement_table[i];
    while ((startpoint = str.find(pair.first, startpoint)) != std::string::npos) {
      str.replace(startpoint, 1, pair.second);
      startpoint += pair.second.length();
    }
  }

  return str;
}
}  // namespace waybar::util
