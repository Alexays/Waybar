#pragma once

#include <chrono>

#if HAVE_CHRONO_TIMEZONES
#include <format>
#else
#include <date/tz.h>
#include <fmt/format.h>

#include <regex>
#endif

// Date
namespace date {
#if HAVE_CHRONO_TIMEZONES
using namespace std::chrono;
using std::format;
#else

using system_clock = std::chrono::system_clock;
using seconds = std::chrono::seconds;

template <typename T>
inline auto format(const char* spec, const T& arg) {
  return date::format(std::regex_replace(spec, std::regex("\\{:L|\\}"), ""), arg);
}

template <typename T>
inline auto format(const std::locale& loc, const char* spec, const T& arg) {
  return date::format(loc, std::regex_replace(spec, std::regex("\\{:L|\\}"), ""), arg);
}
#endif
}  // namespace date

// Format
namespace waybar::util::date::format {
#if HAVE_CHRONO_TIMEZONES
using namespace std;
#else
using namespace fmt;
#endif
}  // namespace waybar::util::date::format

#if not HAVE_CHRONO_TIMEZONES
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
  auto format(const date::zoned_time<Duration, TimeZonePtr>& ztime, FormatContext& ctx) const {
    if (ctx.locale()) {
      const auto loc = ctx.locale().template get<std::locale>();
      return fmt::format_to(ctx.out(), "{}", date::format(loc, fmt::to_string(specs), ztime));
    }
    return fmt::format_to(ctx.out(), "{}", date::format(fmt::to_string(specs), ztime));
  }
};
#endif
