#include "modules/clock.hpp"

#include <spdlog/spdlog.h>
#include <time.h>

#include <sstream>
#include <type_traits>
#ifdef HAVE_LANGINFO_1STDAY
#include <langinfo.h>
#include <locale.h>
#endif

namespace waybar::modules {

Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", "{calendar}", 60), fixed_time_zone_(false) {
  if (config_["timezone"].isString()) {
    spdlog::warn(
        "As using a timezone, some format args may be missing as the date library havn't got a "
        "release since 2018.");
    time_zone_ = date::locate_zone(config_["timezone"].asString());
    fixed_time_zone_ = true;
  }

  if (config_["locale"].isString()) {
    locale_ = std::locale(config_["locale"].asString());
  } else {
    locale_ = std::locale("");
  }

  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now + interval_);
    auto diff = std::chrono::seconds(timeout.time_since_epoch().count() % interval_.count());
    thread_.sleep_until(timeout - diff);
  };
}

auto Clock::update(std::string format, fmt::dynamic_format_arg_store<fmt::format_context>& args)
    -> void {
  // Default to fmt when no timezone is specified
  if (!fixed_time_zone_) {
    tzset();
    auto localtime = fmt::localtime(std::time(nullptr));
    args.push_back(localtime);

    if (AModule::tooltipEnabled()) {
      auto calendarArg = fmt::arg("calendar", localtime);
      args.push_back(std::cref(calendarArg));
    }
  } else {
    auto now = std::chrono::system_clock::now();
    auto nowSecs = date::floor<std::chrono::seconds>(now);
    waybar_time wtime = {locale_, date::make_zoned(time_zone_, nowSecs)};
    args.push_back(wtime);

    if (AModule::tooltipEnabled()) {
      const auto calendar = calendar_text(wtime);
      auto calendarArg = fmt::arg("calendar", calendar);
      args.push_back(std::cref(calendarArg));
    }
  }

  // Call parent update
  ALabel::update(format, args);
}

auto Clock::calendar_text(const waybar_time& wtime) -> std::string {
  const auto daypoint = date::floor<date::days>(wtime.ztime.get_local_time());
  const auto ymd = date::year_month_day(daypoint);
  if (cached_calendar_ymd_ == ymd) {
    return cached_calendar_text_;
  }

  const date::year_month ym(ymd.year(), ymd.month());
  const auto curr_day = ymd.day();

  std::stringstream os;
  const auto first_dow = first_day_of_week();
  weekdays_header(first_dow, os);

  // First week prefixed with spaces if needed.
  auto wd = date::weekday(ym / 1);
  auto empty_days = (wd - first_dow).count();
  if (empty_days > 0) {
    os << std::string(empty_days * 3 - 1, ' ');
  }
  auto last_day = (ym / date::literals::last).day();
  for (auto d = date::day(1); d <= last_day; ++d, ++wd) {
    if (wd != first_dow) {
      os << ' ';
    } else if (unsigned(d) != 1) {
      os << '\n';
    }
    if (d == curr_day) {
      os << "<b><u>" << date::format("%e", d) << "</u></b>";
    } else {
      os << date::format("%e", d);
    }
  }

  auto result = os.str();
  cached_calendar_ymd_ = ymd;
  cached_calendar_text_ = result;
  return result;
}

auto Clock::weekdays_header(const date::weekday& first_dow, std::ostream& os) -> void {
  auto wd = first_dow;
  do {
    if (wd != first_dow) os << ' ';
    Glib::ustring wd_ustring(date::format(locale_, "%a", wd));
    auto wd_len = wd_ustring.length();
    if (wd_len > 2) {
      wd_ustring = wd_ustring.substr(0, 2);
      wd_len = 2;
    }
    const std::string pad(2 - wd_len, ' ');
    os << pad << wd_ustring;
  } while (++wd != first_dow);
  os << "\n";
}

#ifdef HAVE_LANGINFO_1STDAY
template <auto fn>
using deleter_from_fn = std::integral_constant<decltype(fn), fn>;

template <typename T, auto fn>
using deleting_unique_ptr = std::unique_ptr<T, deleter_from_fn<fn>>;
#endif

// Computations done similarly to Linux cal utility.
auto Clock::first_day_of_week() -> date::weekday {
#ifdef HAVE_LANGINFO_1STDAY
  deleting_unique_ptr<std::remove_pointer<locale_t>::type, freelocale> posix_locale{
      newlocale(LC_ALL, locale_.name().c_str(), nullptr)};
  if (posix_locale) {
    const int i = (std::intptr_t)nl_langinfo_l(_NL_TIME_WEEK_1STDAY, posix_locale.get());
    auto ymd = date::year(i / 10000) / (i / 100 % 100) / (i % 100);
    auto wd = date::weekday(ymd);
    uint8_t j = *nl_langinfo_l(_NL_TIME_FIRST_WEEKDAY, posix_locale.get());
    return wd + date::days(j - 1);
  }
#endif
  return date::Sunday;
}

}  // namespace waybar::modules

template <>
struct fmt::formatter<waybar::modules::waybar_time> : fmt::formatter<std::tm> {
  template <typename FormatContext>
  auto format(const waybar::modules::waybar_time& t, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", date::format(t.locale, fmt::to_string(tm_format), t.ztime));
  }
};
