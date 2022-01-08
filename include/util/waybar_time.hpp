#pragma once

#include <fmt/format.h>
#if FMT_VERSION < 60000
#include <fmt/time.h>
#else
#include <fmt/chrono.h>
#endif
#include <date/tz.h>

namespace waybar {

struct waybar_time {
  std::locale         locale;
  date::zoned_seconds ztime;
};

}  // namespace waybar

template <>
struct fmt::formatter<waybar::waybar_time> : fmt::formatter<std::tm> {
  template <typename FormatContext>
  auto format(const waybar::waybar_time& t, FormatContext& ctx) {
#if FMT_VERSION >= 80000
    auto& tm_format = specs;
#endif
    return format_to(ctx.out(), "{}", date::format(t.locale, fmt::to_string(tm_format), t.ztime));
  }
};
