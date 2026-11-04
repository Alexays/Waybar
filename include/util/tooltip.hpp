#pragma once

#include <glibmm/markup.h>
#include <glibmm/ustring.h>
#include <pango/pango.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace waybar::util {

// GDK crashes on a tooltip window over 32767 px on a side and Pango lays out some text in quadratic
// time, so tooltip text from other programs is cut to these limits.
inline constexpr glong tooltip_max_chars = 4096;
inline constexpr glong tooltip_max_lines = 128;
// Characters in a row without a line break opportunity, which the tooltip label cannot wrap
inline constexpr glong tooltip_max_word = 128;

// Cuts plain tooltip text in place to the limits above, ending it with "...".
// Returns whether it cut.
inline bool limit_tooltip_text(std::string& text) {
  // Like GTK, read up to the first NUL; Pango draws what precedes the first invalid byte.
  const char* const str = text.c_str();
  const char* end = nullptr;
  g_utf8_validate(str, -1, &end);
  if (end - str <= tooltip_max_word) {
    return false;
  }

  const glong chars = std::min(g_utf8_strlen(str, end - str), tooltip_max_chars + 1);
  std::vector<PangoLogAttr> attrs(chars + 1);
  pango_get_log_attrs(str, g_utf8_offset_to_pointer(str, chars) - str, -1,
                      pango_language_get_default(), attrs.data(), attrs.size());

  glong lines = 1;
  glong word = 0;
  for (glong i = 0; i < chars; ++i) {
    lines += attrs[i].is_mandatory_break;
    word = attrs[i].is_line_break ? 1 : word + 1;
    if (i == tooltip_max_chars || lines > tooltip_max_lines || word > tooltip_max_word) {
      text.resize(g_utf8_offset_to_pointer(str, i) - str);
      text += "...";
      return true;
    }
  }
  return false;
}

// The same for valid UTF-8 Pango markup, counting its text. Markup that has to be cut cannot be cut
// at an arbitrary place, so it is replaced by its cut text, escaped.
inline void limit_tooltip_markup(std::string& markup) {
  // The text of a markup is never longer than the markup
  const size_t size = std::strlen(markup.c_str());
  if (size <= tooltip_max_word) {
    return;
  }

  // Not pango_parse_markup(): it rejects the <a> links that a GtkLabel accepts
  struct : Glib::Markup::Parser {
    void on_text(Glib::Markup::ParseContext& /*context*/, const Glib::ustring& str) override {
      text += str.raw();
    }
    std::string text;
  } parser;

  try {
    Glib::Markup::ParseContext context(parser);
    context.parse("<markup>");
    context.parse(markup.c_str(), markup.c_str() + size);
    context.parse("</markup>");
    context.end_parse();
  } catch (const Glib::MarkupError&) {
    // GTK shows nothing for markup it cannot parse
    return;
  }

  if (limit_tooltip_text(parser.text)) {
    markup = Glib::Markup::escape_text(parser.text).raw();
  }
}

}  // namespace waybar::util
