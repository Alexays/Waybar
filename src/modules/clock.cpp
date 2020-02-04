#include "modules/clock.hpp"
#include <sstream>
#ifdef HAVE_LANGINFO_1STDAY
#include <langinfo.h>
#endif

using zoned_time = date::zoned_time<std::chrono::system_clock::duration>;

struct waybar_time {
  std::locale locale;
  zoned_time ztime;
};

namespace {

#ifdef HAVE_LANGINFO_1STDAY
// Computations done similarly to Linux cal utility.
date::weekday first_day_of_week() {
  const int i = (std::intptr_t) nl_langinfo(_NL_TIME_WEEK_1STDAY);
  auto ymd = date::year(i / 10000)/(i / 100 % 100)/(i % 100);
  auto wd = date::weekday(ymd);
  uint8_t j = *nl_langinfo(_NL_TIME_FIRST_WEEKDAY);
  return wd + date::days(j - 1);
}
#endif

void weekdays_header(const std::locale& locale, const date::weekday& first_dow, std::ostream& os) {
  auto wd = first_dow;
  do {
    if (wd != first_dow) os << ' ';
    Glib::ustring wd_ustring(date::format(locale, "%a", wd));
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

struct CachedCalendar {
  date::year_month_day ymd;
  std::string text;

  void set(const date::year_month_day& ymd_, std::string text_) {
    ymd = ymd_;
    text = text_;
  }
};

CachedCalendar cached_calendar;

std::string calendar_text(const waybar_time& wtime) {
  const auto daypoint = date::floor<date::days>(wtime.ztime.get_local_time());
  const auto ymd = date::year_month_day(daypoint);
  if (cached_calendar.ymd == ymd) {
    return cached_calendar.text;
  }

  const date::year_month ym(ymd.year(), ymd.month());
  const auto curr_day = ymd.day();

  std::stringstream os;
#ifdef HAVE_LANGINFO_1STDAY
  const auto first_dow = first_day_of_week();
#else
  const auto first_dow = date::Sunday;
#endif
  weekdays_header(wtime.locale, first_dow, os);

  // First week prefixed with spaces if needed.
  auto wd = date::weekday(ym/1);
  auto empty_days = (wd - first_dow).count();
  if (empty_days > 0) {
    os << std::string(empty_days * 3 - 1, ' ');
  }
  auto last_day = (ym/date::literals::last).day();
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
  cached_calendar.set(ymd, result);
  return result;
}

}

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60)
    , fixed_time_zone_(false)
{
  if (config_["timezone"].isString()) {
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

auto waybar::modules::Clock::update() -> void {
  if (!fixed_time_zone_) {
    // Time zone can change. Be sure to pick that.
    time_zone_ = date::current_zone();
  }
  auto now = std::chrono::system_clock::now();
  waybar_time wtime = {locale_, date::make_zoned(time_zone_, now)};

  auto text = fmt::format(format_, wtime);
  label_.set_markup(text);

  if (tooltipEnabled()) {
    if (config_["tooltip-format"].isString()) {
      const auto calendar = calendar_text(wtime);
      auto tooltip_format = config_["tooltip-format"].asString();
      auto tooltip_text = fmt::format(tooltip_format, wtime, fmt::arg("calendar", calendar));
      label_.set_tooltip_markup(tooltip_text);
    } else {
      label_.set_tooltip_markup(text);
    }
  }
}

template <>
struct fmt::formatter<waybar_time> : fmt::formatter<std::tm> {
  template <typename FormatContext>
  auto format(const waybar_time& t, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", date::format(t.locale, fmt::to_string(tm_format), t.ztime));
  }
};
