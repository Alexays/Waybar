#include "modules/clock.hpp"

#include <fmt/chrono.h>
#include <spdlog/spdlog.h>

#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <type_traits>

#include "util/ustring_clen.hpp"
#ifdef HAVE_LANGINFO_1STDAY
#include <langinfo.h>
#include <locale.h>
#endif

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60, false, false, true),
      current_time_zone_idx_{0},
      is_calendar_in_tooltip_{false},
      is_timezoned_list_in_tooltip_{false} {
  if (config_["timezones"].isArray() && !config_["timezones"].empty()) {
    for (const auto& zone_name : config_["timezones"]) {
      if (!zone_name.isString()) continue;
      if (zone_name.asString().empty())
        // local time should be shown
        time_zones_.push_back(date::current_zone());
      else
        try {
          time_zones_.push_back(date::locate_zone(zone_name.asString()));
        } catch (const std::exception& e) {
          spdlog::warn("Timezone: {0}. {1}", zone_name.asString(), e.what());
        }
    }
  } else if (config_["timezone"].isString()) {
    if (config_["timezone"].asString().empty())
      time_zones_.push_back(date::current_zone());
    else
      try {
        time_zones_.push_back(date::locate_zone(config_["timezone"].asString()));
      } catch (const std::exception& e) {
        spdlog::warn("Timezone: {0}. {1}", config_["timezone"].asString(), e.what());
      }
  }

  // If all timezones are parsed and no one is good
  if (!time_zones_.size()) {
    time_zones_.push_back(date::current_zone());
  }

  // Check if a particular placeholder is present in the tooltip format, to know what to calculate
  // on update.
  if (config_["tooltip-format"].isString()) {
    std::string trimmed_format{config_["tooltip-format"].asString()};
    fmtMap_.insert({5, trimmed_format});
    trimmed_format.erase(std::remove_if(trimmed_format.begin(), trimmed_format.end(),
                                        [](unsigned char x) { return std::isspace(x); }),
                         trimmed_format.end());

    if (trimmed_format.find("{" + kCalendarPlaceholder + "}") != std::string::npos) {
      is_calendar_in_tooltip_ = true;
    }
    if (trimmed_format.find("{" + KTimezonedTimeListPlaceholder + "}") != std::string::npos) {
      is_timezoned_list_in_tooltip_ = true;
    }
  }

  // Calendar configuration
  if (is_calendar_in_tooltip_) {
    if (config_[kCalendarPlaceholder]["weeks-pos"].isString()) {
      if (config_[kCalendarPlaceholder]["weeks-pos"].asString() == "left") {
        cldWPos_ = WeeksSide::LEFT;
      } else if (config_[kCalendarPlaceholder]["weeks-pos"].asString() == "right") {
        cldWPos_ = WeeksSide::RIGHT;
      }
    }
    if (config_[kCalendarPlaceholder]["format"]["months"].isString())
      fmtMap_.insert({0, config_[kCalendarPlaceholder]["format"]["months"].asString()});
    else
      fmtMap_.insert({0, "{}"});
    if (config_[kCalendarPlaceholder]["format"]["days"].isString())
      fmtMap_.insert({2, config_[kCalendarPlaceholder]["format"]["days"].asString()});
    else
      fmtMap_.insert({2, "{}"});
    if (config_[kCalendarPlaceholder]["format"]["weeks"].isString() &&
        cldWPos_ != WeeksSide::HIDDEN) {
      fmtMap_.insert(
          {4, std::regex_replace(config_[kCalendarPlaceholder]["format"]["weeks"].asString(),
                                 std::regex("\\{\\}"),
                                 (first_day_of_week() == date::Monday) ? "{:%W}" : "{:%U}")});
      Glib::ustring tmp{std::regex_replace(fmtMap_[4], std::regex("</?[^>]+>|\\{.*\\}"), "")};
      cldWnLen_ += tmp.size();
    } else {
      if (cldWPos_ != WeeksSide::HIDDEN)
        fmtMap_.insert({4, (first_day_of_week() == date::Monday) ? "{:%W}" : "{:%U}"});
      else
        cldWnLen_ = 0;
    }
    if (config_[kCalendarPlaceholder]["format"]["weekdays"].isString())
      fmtMap_.insert({1, config_[kCalendarPlaceholder]["format"]["weekdays"].asString()});
    else
      fmtMap_.insert({1, "{}"});
    if (config_[kCalendarPlaceholder]["format"]["today"].isString()) {
      fmtMap_.insert({3, config_[kCalendarPlaceholder]["format"]["today"].asString()});
      cldBaseDay_ =
          date::year_month_day{date::floor<date::days>(std::chrono::system_clock::now())}.day();
    } else
      fmtMap_.insert({3, "{}"});
    if (config_[kCalendarPlaceholder]["mode"].isString()) {
      const std::string cfgMode{(config_[kCalendarPlaceholder]["mode"].isString())
                                    ? config_[kCalendarPlaceholder]["mode"].asString()
                                    : "month"};
      const std::map<std::string, const CldMode&> monthModes{{"month", CldMode::MONTH},
                                                             {"year", CldMode::YEAR}};
      if (monthModes.find(cfgMode) != monthModes.end())
        cldMode_ = monthModes.at(cfgMode);
      else
        spdlog::warn(
            "Clock calendar configuration \"mode\"\"\" \"{0}\" is not recognized. Mode = \"month\" "
            "is using instead",
            cfgMode);
    }
    if (config_[kCalendarPlaceholder]["mode-mon-col"].isInt()) {
      cldMonCols_ = config_[kCalendarPlaceholder]["mode-mon-col"].asInt();
      if (cldMonCols_ == 0u || 12 % cldMonCols_ != 0u) {
        cldMonCols_ = 3u;
        spdlog::warn(
            "Clock calendar configuration \"mode-mon-col\" = {0} must be one of [1, 2, 3, 4, 6, "
            "12]. Value 3 is using instead",
            cldMonCols_);
      }
    } else
      cldMonCols_ = 1;
    if (config_[kCalendarPlaceholder]["on-scroll"].isInt()) {
      cldShift_ = date::months{config_[kCalendarPlaceholder]["on-scroll"].asInt()};
      event_box_.add_events(Gdk::LEAVE_NOTIFY_MASK);
      event_box_.signal_leave_notify_event().connect([this](GdkEventCrossing*) {
        cldCurrShift_ = date::months{0};
        return false;
      });
    }
  }

  if (config_["locale"].isString())
    locale_ = std::locale(config_["locale"].asString());
  else
    locale_ = std::locale("");

  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    /* difference with projected wakeup time */
    auto diff = now.time_since_epoch() % interval_;
    /* sleep until the next projected time */
    thread_.sleep_for(interval_ - diff);
  };
}

const date::time_zone* waybar::modules::Clock::current_timezone() {
  return time_zones_[current_time_zone_idx_];
}

auto waybar::modules::Clock::update() -> void {
  const auto* tz{current_timezone()};
  const date::zoned_time now{
      tz,
      date::floor<std::chrono::seconds>(
          std::chrono::system_clock::now())};  // Define local time is based on provided time zone
  const date::year_month_day today{
      date::floor<date::days>(now.get_local_time())};            // Convert now to year_month_day
  const date::year_month_day shiftedDay{today + cldCurrShift_};  // Shift today
  // Define shift local time
  const auto shiftedNow{date::make_zoned(
      tz, date::local_days(shiftedDay) +
              (now.get_local_time() - date::floor<date::days>(now.get_local_time())))};

  label_.set_markup(fmt::format(locale_, fmt::runtime(format_), now));

  if (tooltipEnabled()) {
    const std::string tz_text{(is_timezoned_list_in_tooltip_) ? timezones_text(now.get_sys_time())
                                                              : ""};
    const std::string cld_text{(is_calendar_in_tooltip_) ? get_calendar(today, shiftedDay, tz)
                                                         : ""};

    const std::string text{fmt::format(locale_, fmt::runtime(fmtMap_[5]), shiftedNow,
                                       fmt::arg(KTimezonedTimeListPlaceholder.c_str(), tz_text),
                                       fmt::arg(kCalendarPlaceholder.c_str(), cld_text))};
    label_.set_tooltip_markup(text);
  }

  // Call parent update
  ALabel::update();
}

auto waybar::modules::Clock::doAction(const std::string& name) -> void {
  if ((actionMap_[name])) {
    (this->*actionMap_[name])();
    update();
  } else
    spdlog::error("Clock. Unsupported action \"{0}\"", name);
}

// The number of weeks in calendar month layout plus 1 more for calendar titles
const unsigned cldRowsInMonth(const date::year_month& ym, const date::weekday& firstdow) {
  using namespace date;
  return static_cast<unsigned>(
             ceil<weeks>((weekday{ym / 1} - firstdow) + ((ym / last).day() - day{0})).count()) +
         2;
}

auto cldGetWeekForLine(const date::year_month& ym, const date::weekday& firstdow,
                       unsigned const line) -> const date::year_month_weekday {
  unsigned index = line - 2;
  auto sd = date::sys_days{ym / 1};
  if (date::weekday{sd} == firstdow) ++index;
  auto ymdw = ym / firstdow[index];
  return ymdw;
}

auto getCalendarLine(const date::year_month_day& currDate, const date::year_month ym,
                     const unsigned line, const date::weekday& firstdow,
                     const std::locale* const locale_) -> std::string {
  using namespace date::literals;
  std::ostringstream res;

  switch (line) {
    case 0: {
      // Output month and year title
      res << date::format(*locale_, "%B %Y", ym);
      break;
    }
    case 1: {
      // Output weekday names title
      auto wd{firstdow};
      do {
        Glib::ustring wd_ustring{date::format(*locale_, "%a", wd)};
        auto clen{ustring_clen(wd_ustring)};
        auto wd_len{wd_ustring.length()};
        while (clen > 2) {
          wd_ustring = wd_ustring.substr(0, wd_len - 1);
          --wd_len;
          clen = ustring_clen(wd_ustring);
        }
        const std::string pad(2 - clen, ' ');

        if (wd != firstdow) res << ' ';

        res << pad << wd_ustring;
      } while (++wd != firstdow);
      break;
    }
    case 2: {
      // Output first week prefixed with spaces if necessary
      auto wd = date::weekday{ym / 1};
      res << std::string(static_cast<unsigned>((wd - firstdow).count()) * 3, ' ');

      if (currDate.year() != ym.year() || currDate.month() != ym.month() || currDate != ym / 1_d)
        res << date::format("%e", 1_d);
      else
        res << "{today}";

      auto d = 2_d;

      while (++wd != firstdow) {
        if (currDate.year() != ym.year() || currDate.month() != ym.month() || currDate != ym / d)
          res << date::format(" %e", d);
        else
          res << " {today}";

        ++d;
      }
      break;
    }
    default: {
      // Output a non-first week:
      auto ymdw{cldGetWeekForLine(ym, firstdow, line)};
      if (ymdw.ok()) {
        auto d = date::year_month_day{ymdw}.day();
        auto const e = (ym / last).day();
        auto wd = firstdow;

        if (currDate.year() != ym.year() || currDate.month() != ym.month() || currDate != ym / d)
          res << date::format("%e", d);
        else
          res << "{today}";

        while (++wd != firstdow && ++d <= e) {
          if (currDate.year() != ym.year() || currDate.month() != ym.month() || currDate != ym / d)
            res << date::format(" %e", d);
          else
            res << " {today}";
        }
        // Append row with spaces if the week did not complete
        res << std::string(static_cast<unsigned>((firstdow - wd).count()) * 3, ' ');
      }
      break;
    }
  }

  return res.str();
}

auto waybar::modules::Clock::get_calendar(const date::year_month_day& today,
                                          const date::year_month_day& ymd,
                                          const date::time_zone* tz) -> const std::string {
  const auto ym{ymd.year() / ymd.month()};
  const auto y{ymd.year()};
  const auto d{ymd.day()};
  const auto firstdow = first_day_of_week();
  const auto maxRows{12 / cldMonCols_};
  std::ostringstream os;
  std::ostringstream tmp;

  if (cldMode_ == CldMode::YEAR) {
    if (y / date::month{1} / 1 == cldYearShift_)
      if (d == cldBaseDay_ || (uint)cldBaseDay_ == 0u)
        return cldYearCached_;
      else
        cldBaseDay_ = d;
    else
      cldYearShift_ = y / date::month{1} / 1;
  }
  if (cldMode_ == CldMode::MONTH) {
    if (ym == cldMonShift_)
      if (d == cldBaseDay_ || (uint)cldBaseDay_ == 0u)
        return cldMonCached_;
      else
        cldBaseDay_ = d;
    else
      cldMonShift_ = ym;
  }

  // Compute number of lines needed for each calendar month
  unsigned ml[12]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

  for (auto& m : ml) {
    if (cldMode_ == CldMode::YEAR || m == static_cast<unsigned>(ymd.month()))
      m = cldRowsInMonth(y / date::month{m}, firstdow);
    else
      m = 0u;
  }

  for (auto row{0u}; row < maxRows; ++row) {
    const auto lines = *std::max_element(std::begin(ml) + (row * cldMonCols_),
                                         std::begin(ml) + ((row + 1) * cldMonCols_));
    for (auto line{0u}; line < lines; ++line) {
      for (auto col{0u}; col < cldMonCols_; ++col) {
        const auto mon{date::month{row * cldMonCols_ + col + 1}};
        if (cldMode_ == CldMode::YEAR || y / mon == ym) {
          date::year_month ymTmp{y / mon};
          if (col != 0 && cldMode_ == CldMode::YEAR) os << "   ";

          // Week numbers on the left
          if (cldWPos_ == WeeksSide::LEFT && line > 0) {
            if (line > 1) {
              if (line < ml[static_cast<unsigned>(ymTmp.month()) - 1u])
                os << fmt::format(fmt::runtime(fmtMap_[4]),
                                  (line == 2)
                                      ? date::zoned_seconds{tz, date::local_days{ymTmp / 1}}
                                      : date::zoned_seconds{tz, date::local_days{cldGetWeekForLine(
                                                                    ymTmp, firstdow, line)}})
                   << ' ';
              else
                os << std::string(cldWnLen_, ' ');
            }
          }

          os << fmt::format(
              fmt::runtime((cldWPos_ != WeeksSide::LEFT || line == 0) ? "{:<{}}" : "{:>{}}"),
              getCalendarLine(today, ymTmp, line, firstdow, &locale_),
              (cldMonColLen_ + ((line < 2) ? cldWnLen_ : 0)));

          // Week numbers on the right
          if (cldWPos_ == WeeksSide ::RIGHT && line > 0) {
            if (line > 1) {
              if (line < ml[static_cast<unsigned>(ymTmp.month()) - 1u])
                os << ' '
                   << fmt::format(fmt::runtime(fmtMap_[4]),
                                  (line == 2)
                                      ? date::zoned_seconds{tz, date::local_days{ymTmp / 1}}
                                      : date::zoned_seconds{tz, date::local_days{cldGetWeekForLine(
                                                                    ymTmp, firstdow, line)}});
              else
                os << std::string(cldWnLen_, ' ');
            }
          }
        }
      }

      // Apply user formats to calendar
      if (line < 2)
        tmp << fmt::format(fmt::runtime(fmtMap_[line]), os.str());
      else
        tmp << os.str();
      // Clear ostringstream
      std::ostringstream().swap(os);
      if (line + 1u != lines || (row + 1u != maxRows && cldMode_ == CldMode::YEAR)) tmp << '\n';
    }
    if (row + 1u != maxRows && cldMode_ == CldMode::YEAR) tmp << '\n';
  }

  os << fmt::format(  // Apply days format
      fmt::runtime(fmt::format(fmt::runtime(fmtMap_[2]), tmp.str())),
      // Apply today format
      fmt::arg("today", fmt::format(fmt::runtime(fmtMap_[3]), date::format("%e", d))));

  if (cldMode_ == CldMode::YEAR)
    cldYearCached_ = os.str();
  else
    cldMonCached_ = os.str();

  return os.str();
}

/*Clock actions*/
void waybar::modules::Clock::cldModeSwitch() {
  cldMode_ = (cldMode_ == CldMode::YEAR) ? CldMode::MONTH : CldMode::YEAR;
}
void waybar::modules::Clock::cldShift_up() {
  cldCurrShift_ += ((cldMode_ == CldMode::YEAR) ? 12 : 1) * cldShift_;
}
void waybar::modules::Clock::cldShift_down() {
  cldCurrShift_ -= ((cldMode_ == CldMode::YEAR) ? 12 : 1) * cldShift_;
}
void waybar::modules::Clock::tz_up() {
  auto nr_zones = time_zones_.size();

  if (nr_zones == 1) return;

  size_t new_idx = current_time_zone_idx_ + 1;
  current_time_zone_idx_ = new_idx == nr_zones ? 0 : new_idx;
}
void waybar::modules::Clock::tz_down() {
  auto nr_zones = time_zones_.size();

  if (nr_zones == 1) return;

  current_time_zone_idx_ = current_time_zone_idx_ == 0 ? nr_zones - 1 : current_time_zone_idx_ - 1;
}

auto waybar::modules::Clock::timezones_text(std::chrono::system_clock::time_point now)
    -> std::string {
  if (time_zones_.size() == 1) {
    return "";
  }
  std::stringstream os;
  for (size_t time_zone_idx = 0; time_zone_idx < time_zones_.size(); ++time_zone_idx) {
    if (static_cast<int>(time_zone_idx) == current_time_zone_idx_) {
      continue;
    }
    const date::time_zone* timezone = time_zones_[time_zone_idx];
    if (!timezone) {
      timezone = date::current_zone();
    }
    auto ztime = date::zoned_time{timezone, date::floor<std::chrono::seconds>(now)};
    os << fmt::format(locale_, fmt::runtime(format_), ztime) << '\n';
  }
  return os.str();
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
    auto ymd = date::year(i / 10000) / (i / 100 % 100) / (i % 100);
    auto wd = date::weekday(ymd);
    uint8_t j = *nl_langinfo_l(_NL_TIME_FIRST_WEEKDAY, posix_locale.get());
    return wd + date::days(j - 1);
  }
#endif
  return date::Sunday;
}
