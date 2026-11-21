#pragma once

#include <fmt/format.h>
#include <glib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

// Helpers to put names into sway commands sent over IPC.
namespace waybar::modules::sway::ipc {

// Returns `arg` quoted as a single argument of a sway IPC command, usable
// anywhere in a command list, or std::nullopt if `arg` is not supported.
//
// Supported arguments are valid UTF-8 (sway compiles criteria as PCRE2_UTF
// regexes) without newlines or backslashes:
// - sway's IPC server turns newlines into ';' before parsing;
// - sway never removes backslashes from IPC commands, yet reads `\x` as
//   escaping `x`.
//
// Without backslashes, sway reads a quote as literal inside the other kind of
// quotes, and adjacent quoted segments as a single argument. Each '"' is
// therefore put in single quotes and each '\'' in double quotes, switching
// quotes when needed: a'b"c becomes "a'b"'"c'. '$' is doubled so that sway
// does not expand variables.
inline std::optional<std::string> quote_arg(std::string_view arg) {
  if (arg.find_first_of("\n\\") != std::string_view::npos ||
      g_utf8_validate(arg.data(), static_cast<gssize>(arg.size()), nullptr) == FALSE) {
    return std::nullopt;
  }
  // Start with the quote that the first quote character of `arg` needs.
  const size_t first = arg.find_first_of("\"'");
  char quote = first != std::string_view::npos && arg[first] == '"' ? '\'' : '"';
  std::string out(1, quote);
  for (const char c : arg) {
    if (c == quote) {
      quote = quote == '"' ? '\'' : '"';
      out += c;      // close the current segment...
      out += quote;  // ...and open one with the other quote
    }
    out += c;
    if (c == '$') {
      out += '$';
    }
  }
  out += quote;
  return out;
}

// quote_arg() for a workspace name, also rejecting the names that `workspace
// <name>` reads as a keyword (case-insensitively), whatever the quoting.
inline std::optional<std::string> quote_workspace_name(std::string_view name) {
  static constexpr std::array<std::string_view, 10> reserved = {
      "number",  "next",           "prev",   "next_on_output",           "prev_on_output",
      "current", "back_and_forth", "output", "--no-auto-back-and-forth", "gaps",
  };
  std::string lower(name);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (std::find(reserved.begin(), reserved.end(), lower) != reserved.end()) {
    return std::nullopt;
  }
  return quote_arg(name);
}

// Returns a regex matching `s` literally, for use inside a quoted criteria
// value. Every ASCII character other than a letter or a digit is written as
// \x{HH}, so the result contains no quote and no regex metacharacter.
inline std::string regex_literal(std::string_view s) {
  std::string out;
  for (const char c : s) {
    const auto u = static_cast<unsigned char>(c);
    if (u >= 0x80 || std::isalnum(u) != 0) {
      out += c;
    } else {
      out += fmt::format("\\x{{{:02x}}}", u);
    }
  }
  return out;
}

}  // namespace waybar::modules::sway::ipc
