#include "modules/clock.hpp"

#include <time.h>
#include <spdlog/spdlog.h>

#include <sstream>
#include <type_traits>
#include "util/ustring_clen.hpp"
#ifdef HAVE_LANGINFO_1STDAY
#include <langinfo.h>
#include <locale.h>
#endif

using waybar::modules::waybar_time;

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60, false, false, true),
      current_time_zone_idx_(0),
      is_calendar_in_tooltip_(false)
{
  if (config_["timezones"].isArray() && !config_["timezones"].empty()) {
    for (const auto& zone_name: config_["timezones"]) {
      if (!zone_name.isString() || zone_name.asString().empty()) {
        time_zones_.push_back(nullptr);
        continue;
      }
      time_zones_.push_back(
        date::locate_zone(
          zone_name.asString()
        )
      );
    }
  } else if (config_["timezone"].isString() && !config_["timezone"].asString().empty()) {
    time_zones_.push_back(
        date::locate_zone(
          config_["timezone"].asString()
        )
      );
  }

  // If all timezones are parsed and no one is good, add nullptr to the timezones vector, to mark that local time should be shown.
  if (!time_zones_.size()) {
    time_zones_.push_back(nullptr);
  }

  if (!is_timezone_fixed()) {
    spdlog::warn("As using a timezone, some format args may be missing as the date library haven't got a release since 2018.");
  }

  // Check if a particular placeholder is present in the tooltip format, to know what to calculate on update.
  if (config_["tooltip-format"].isString()) {
    std::string trimmed_format = config_["tooltip-format"].asString();
    trimmed_format.erase(std::remove_if(trimmed_format.begin(),
                              trimmed_format.end(),
                              [](unsigned char x){return std::isspace(x);}),
               trimmed_format.end());
    if (trimmed_format.find("{" + kCalendarPlaceholder + "}") != std::string::npos) {
      is_calendar_in_tooltip_ = true;
    }
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

const date::time_zone* waybar::modules::Clock::current_timezone() {
  return time_zones_[current_time_zone_idx_] ? time_zones_[current_time_zone_idx_] : date::current_zone();
}

bool waybar::modules::Clock::is_timezone_fixed() {
  return time_zones_[current_time_zone_idx_] != nullptr;
}

auto waybar::modules::Clock::update() -> void {
  auto time_zone = current_timezone();
  auto now = std::chrono::system_clock::now();
  waybar_time wtime = {locale_,
                       date::make_zoned(time_zone, date::floor<std::chrono::seconds>(now))};
  std::string text = "";
  if (!is_timezone_fixed()) {
    // As date dep is not fully compatible, prefer fmt
    tzset();
    auto localtime = fmt::localtime(std::chrono::system_clock::to_time_t(now));
    text = fmt::format(format_, localtime);
  } else {
    text = fmt::format(format_, wtime);
  }
  label_.set_markup(text);

  if (tooltipEnabled()) {
    if (config_["tooltip-format"].isString()) {
      std::string calendar_lines = "";
      if (is_calendar_in_tooltip_) {
        calendar_lines = calendar_text(wtime);
      }
      auto tooltip_format = config_["tooltip-format"].asString();
      text = fmt::format(tooltip_format, wtime, fmt::arg(kCalendarPlaceholder.c_str(), calendar_lines));
    }
  }

  label_.set_tooltip_markup(text);
  // Call parent update
  ALabel::update();
}

bool waybar::modules::Clock::handleScroll(GdkEventScroll *e) {
  // defer to user commands if set
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScroll(e);
  }

  auto dir = AModule::getScrollDir(e);
  if (dir != SCROLL_DIR::UP && dir != SCROLL_DIR::DOWN) {
    return true;
  }
  if (time_zones_.size() == 1) {
    return true;
  }

  auto nr_zones = time_zones_.size();
  if (dir == SCROLL_DIR::UP) {
    size_t new_idx = current_time_zone_idx_ + 1;
    current_time_zone_idx_ = new_idx == nr_zones ? 0 : new_idx;
  } else {
    current_time_zone_idx_ = current_time_zone_idx_ == 0 ? nr_zones - 1 : current_time_zone_idx_ - 1;
  }

  update();
  return true;
}

auto waybar::modules::Clock::calendar_text(const waybar_time& wtime) -> std::string {
  const auto daypoint = date::floor<date::days>(wtime.ztime.get_local_time());
  const auto ymd = date::year_month_day(daypoint);
  if (cached_calendar_ymd_ == ymd) {
    return cached_calendar_text_;
  }

  const date::year_month ym(ymd.year(), ymd.month());
  const auto             curr_day = ymd.day();

  std::stringstream os;
  const auto        first_dow = first_day_of_week();
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
      if (config_["today-format"].isString()) {
        auto today_format = config_["today-format"].asString();
        os << fmt::format(today_format, date::format("%e", d));
      } else {
        os << "<b><u>" << date::format("%e", d) << "</u></b>";
      }
    } else {
      os << date::format("%e", d);
    }
  }

  auto result = os.str();
  cached_calendar_ymd_ = ymd;
  cached_calendar_text_ = result;
  return result;
}

auto waybar::modules::Clock::weekdays_header(const date::weekday& first_dow, std::ostream& os)
    -> void {
  auto wd = first_dow;
  do {
    if (wd != first_dow) os << ' ';
    Glib::ustring wd_ustring(date::format(locale_, "%a", wd));
    auto clen = ustring_clen(wd_ustring);
    auto wd_len = wd_ustring.length();
    while (clen > 2) {
      wd_ustring = wd_ustring.substr(0, wd_len-1);
      wd_len--;
      clen = ustring_clen(wd_ustring);
    }
    const std::string pad(2 - clen, ' ');
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
auto waybar::modules::Clock::first_day_of_week() -> date::weekday {
#ifdef HAVE_LANGINFO_1STDAY
  deleting_unique_ptr<std::remove_pointer<locale_t>::type, freelocale> posix_locale{
      newlocale(LC_ALL, locale_.name().c_str(), nullptr)};
  if (posix_locale) {
    const int i = (std::intptr_t)nl_langinfo_l(_NL_TIME_WEEK_1STDAY, posix_locale.get());
    auto      ymd = date::year(i / 10000) / (i / 100 % 100) / (i % 100);
    auto      wd = date::weekday(ymd);
    uint8_t   j = *nl_langinfo_l(_NL_TIME_FIRST_WEEKDAY, posix_locale.get());
    return wd + date::days(j - 1);
  }
#endif
  return date::Sunday;
}

template <>
struct fmt::formatter<waybar_time> : fmt::formatter<std::tm> {
  template <typename FormatContext>
  auto format(const waybar_time& t, FormatContext& ctx) {
#if FMT_VERSION >= 80000
  auto& tm_format = specs;
#endif
    return format_to(ctx.out(), "{}", date::format(t.locale, fmt::to_string(tm_format), t.ztime));
  }
};
