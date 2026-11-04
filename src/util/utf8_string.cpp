#include <glib.h>

#include <algorithm>
#include <string>
#include <util/utf8_string.hpp>

namespace waybar::util {

namespace {
// Wide characters count as two, zero-width characters count as zero, but a string is at least as
// wide as its number of zero-width characters: fonts draw some of them, and any run would fit
// Modifies str in-place (unless width = std::string::npos)
// Returns the total width of the string pre-truncating
size_t measure_and_truncate(std::string& str, size_t width = std::string::npos) {
  if (str.length() == 0) return 0;

  // With a width, keep nothing unless a non-space character fits, even after leading spaces
  const gchar* trunc_end = width != std::string::npos ? str.data() : nullptr;

  size_t total_width = 0;
  size_t columns = 0;
  size_t zero_width = 0;

  for (gchar *data = str.data(), *end = data + str.size(); data != nullptr;) {
    gunichar c = g_utf8_get_char_validated(data, end - data);
    if (c == -1U || c == -2U) {
      // invalid unicode, treat string as ascii
      if (width != std::string::npos && str.length() > width) str.resize(width);
      return str.length();
    } else if (g_unichar_iswide(c)) {
      columns += 2;
    } else if (!g_unichar_iszerowidth(c) && c != 0xAD) {  // neither zero-width nor soft hyphen
      columns += 1;
    } else {
      zero_width += 1;
    }
    total_width = std::max(columns, zero_width);

    data = g_utf8_find_next_char(data, end);
    if (width != std::string::npos && total_width <= width && !g_unichar_isspace(c))
      trunc_end = data;
  }

  if (trunc_end) str.resize(trunc_end - str.data());

  return total_width;
}
}  // namespace

size_t utf8_width(const std::string& str) {
  return measure_and_truncate(const_cast<std::string&>(str));
}

void utf8_truncate(std::string& s, const std::string& ellipsis, size_t max_len) {
  if (max_len == 0) {
    s.resize(0);
    return;
  }
  size_t len = measure_and_truncate(s, max_len);
  if (len > max_len) {
    size_t ellipsis_len = utf8_width(ellipsis);
    if (max_len >= ellipsis_len) {
      if (ellipsis_len) measure_and_truncate(s, max_len - ellipsis_len);
      s += ellipsis;
    } else {
      s.resize(0);
    }
  }
}
}  // namespace waybar::util
