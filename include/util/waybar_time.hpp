#pragma once

#include <date/tz.h>
#include <fmt/format.h>

namespace waybar {

struct waybar_time {
  std::locale locale;
  date::zoned_seconds ztime;
};

}  // namespace waybar

template <>
struct fmt::formatter<waybar::waybar_time> {
  std::string_view specs;

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it == ':') {
      ++it;
    }
    auto end = it;
    while (end != ctx.end() && *end != '}') {
      ++end;
    }
    if (end != it) {
      specs = {it, std::string_view::size_type(end - it)};
    }
    return end;
  }

  template <typename FormatContext>
  auto format(const waybar::waybar_time& t, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", date::format(t.locale, fmt::to_string(specs), t.ztime));
  }
};
