#pragma once

#include <fmt/format.h>

#if HAVE_CHRONO_TIMEZONES
#include <chrono>
#include <format>

/* Compatibility layer for <date/tz.h> on top of C++20 <chrono> */
namespace date {

using namespace std::chrono;

namespace literals {
using std::chrono::last;
}

inline auto format(const std::string& spec, const auto& ztime) {
  return spec.empty() ? "" : std::vformat("{:L" + spec + "}", std::make_format_args(ztime));
}

inline auto format(const std::locale& loc, const std::string& spec, const auto& ztime) {
  return spec.empty() ? "" : std::vformat(loc, "{:L" + spec + "}", std::make_format_args(ztime));
}

}  // namespace date

#else
#include <date/tz.h>
#endif

template <typename Duration, typename TimeZonePtr>
struct fmt::formatter<date::zoned_time<Duration, TimeZonePtr>> {
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
  auto format(const date::zoned_time<Duration, TimeZonePtr>& ztime, FormatContext& ctx) {
    if (ctx.locale()) {
      const auto loc = ctx.locale().template get<std::locale>();
      return fmt::format_to(ctx.out(), "{}", date::format(loc, fmt::to_string(specs), ztime));
    }
    return fmt::format_to(ctx.out(), "{}", date::format(fmt::to_string(specs), ztime));
  }
};
