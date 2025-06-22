#include "modules/clock.hpp"

#include <glib.h>
#include <gtkmm/tooltip.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>

#include "util/ustring_clen.hpp"

#ifdef HAVE_LANGINFO_1STDAY
#include <langinfo.h>

#include <clocale>
#endif

using namespace date;
namespace fmt_lib = waybar::util::date::format;

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60, false, false, true),
      m_locale_{std::locale(config_["locale"].isString() ? config_["locale"].asString() : "")},
      m_tlpFmt_{(config_["tooltip-format"].isString()) ? config_["tooltip-format"].asString() : ""},
      m_tooltip_{new Gtk::Label()},
      cldInTooltip_{m_tlpFmt_.find("{" + kCldPlaceholder + "}") != std::string::npos},
      cldYearShift_{January / 1 / 1900},
      cldMonShift_{year(1900) / January},
      tzInTooltip_{m_tlpFmt_.find("{" + kTZPlaceholder + "}") != std::string::npos},
      tzCurrIdx_{0},
      ordInTooltip_{m_tlpFmt_.find("{" + kOrdPlaceholder + "}") != std::string::npos} {
  m_tlpText_ = m_tlpFmt_;

  if (config_["timezones"].isArray() && !config_["timezones"].empty()) {
    for (const auto& zone_name : config_["timezones"]) {
      if (!zone_name.isString()) continue;
      if (zone_name.asString().empty())
        // local time should be shown
        tzList_.push_back(nullptr);
      else
        try {
          tzList_.push_back(locate_zone(zone_name.asString()));
        } catch (const std::exception& e) {
          spdlog::warn("Timezone: {0}. {1}", zone_name.asString(), e.what());
        }
    }
  } else if (config_["timezone"].isString()) {
    if (config_["timezone"].asString().empty())
      // local time should be shown
      tzList_.push_back(nullptr);
    else
      try {
        tzList_.push_back(locate_zone(config_["timezone"].asString()));
      } catch (const std::exception& e) {
        spdlog::warn("Timezone: {0}. {1}", config_["timezone"].asString(), e.what());
      }
  }
  if (!tzList_.size()) tzList_.push_back(nullptr);

  // Calendar properties
  if (cldInTooltip_) {
    if (config_[kCldPlaceholder]["mode"].isString()) {
      const std::string cfgMode{config_[kCldPlaceholder]["mode"].asString()};
      const std::map<std::string_view, const CldMode&> monthModes{{"month", CldMode::MONTH},
                                                                  {"year", CldMode::YEAR}};
      if (monthModes.find(cfgMode) != monthModes.end())
        cldMode_ = monthModes.at(cfgMode);
      else
        spdlog::warn(
            "Clock calendar configuration mode \"{0}\" is not recognized. Mode = \"month\" is "
            "using instead",
            cfgMode);
    }
    if (config_[kCldPlaceholder]["weeks-pos"].isString()) {
      if (config_[kCldPlaceholder]["weeks-pos"].asString() == "left") cldWPos_ = WS::LEFT;
      if (config_[kCldPlaceholder]["weeks-pos"].asString() == "right") cldWPos_ = WS::RIGHT;
    }
    if (config_[kCldPlaceholder]["format"]["months"].isString())
      fmtMap_.insert({0, config_[kCldPlaceholder]["format"]["months"].asString()});
    else
      fmtMap_.insert({0, "{}"});
    if (config_[kCldPlaceholder]["format"]["weekdays"].isString())
      fmtMap_.insert({1, config_[kCldPlaceholder]["format"]["weekdays"].asString()});
    else
      fmtMap_.insert({1, "{}"});

    if (config_[kCldPlaceholder]["format"]["days"].isString())
      fmtMap_.insert({2, config_[kCldPlaceholder]["format"]["days"].asString()});
    else
      fmtMap_.insert({2, "{}"});
    if (config_[kCldPlaceholder]["format"]["today"].isString()) {
      fmtMap_.insert({3, config_[kCldPlaceholder]["format"]["today"].asString()});
      cldBaseDay_ =
          year_month_day{
              floor<days>(zoned_time{local_zone(), system_clock::now()}.get_local_time())}
              .day();
    } else
      fmtMap_.insert({3, "{}"});
    if (config_[kCldPlaceholder]["format"]["weeks"].isString() && cldWPos_ != WS::HIDDEN) {
      fmtMap_.insert({4, std::regex_replace(config_[kCldPlaceholder]["format"]["weeks"].asString(),
                                            std::regex("\\{\\}"),
                                            (first_day_of_week() == Monday) ? "{:%W}" : "{:%U}")});
      Glib::ustring tmp{std::regex_replace(fmtMap_[4], std::regex("</?[^>]+>|\\{.*\\}"), "")};
      cldWnLen_ += tmp.size();
    } else {
      if (cldWPos_ != WS::HIDDEN)
        fmtMap_.insert({4, (first_day_of_week() == Monday) ? "{:%W}" : "{:%U}"});
      else
        cldWnLen_ = 0;
    }
    if (config_[kCldPlaceholder]["mode-mon-col"].isInt()) {
      cldMonCols_ = config_[kCldPlaceholder]["mode-mon-col"].asInt();
      if (cldMonCols_ == 0u || (12 % cldMonCols_) != 0u) {
        spdlog::warn(
            "Clock calendar configuration mode-mon-col = {0} must be one of [1, 2, 3, 4, 6, 12]. "
            "Value 3 is using instead",
            cldMonCols_);
        cldMonCols_ = 3u;
      }
    } else
      cldMonCols_ = 1;
    if (config_[kCldPlaceholder]["on-scroll"].isInt()) {
      cldShift_ = config_[kCldPlaceholder]["on-scroll"].asInt();
      event_box_.add_events(Gdk::LEAVE_NOTIFY_MASK);
      event_box_.signal_leave_notify_event().connect([this](GdkEventCrossing*) {
        cldCurrShift_ = months{0};
        return false;
      });
    }
  }

  if (tooltipEnabled()) {
    label_.set_has_tooltip(true);
    label_.signal_query_tooltip().connect(sigc::mem_fun(*this, &Clock::query_tlp_cb));
  }

  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_ - system_clock::now().time_since_epoch() % interval_);
  };
}

bool waybar::modules::Clock::query_tlp_cb(int, int, bool,
                                          const Glib::RefPtr<Gtk::Tooltip>& tooltip) {
  tooltip->set_custom(*m_tooltip_.get());
  return true;
}

auto waybar::modules::Clock::update() -> void {
  const auto* tz = tzList_[tzCurrIdx_] != nullptr ? tzList_[tzCurrIdx_] : local_zone();
  const zoned_time now{tz, floor<seconds>(system_clock::now())};

  label_.set_markup(fmt_lib::vformat(m_locale_, format_, fmt_lib::make_format_args(now)));

  if (tooltipEnabled()) {
    const year_month_day today{floor<days>(now.get_local_time())};
    const auto shiftedDay{today + cldCurrShift_};
    const zoned_time shiftedNow{
        tz, local_days(shiftedDay) + (now.get_local_time() - floor<days>(now.get_local_time()))};

    if (tzInTooltip_) tzText_ = getTZtext(now.get_sys_time());
    if (cldInTooltip_) cldText_ = get_calendar(today, shiftedDay, tz);
    if (ordInTooltip_) ordText_ = get_ordinal_date(shiftedDay);
    if (tzInTooltip_ || cldInTooltip_ || ordInTooltip_) {
      // std::vformat doesn't support named arguments.
      m_tlpText_ =
          std::regex_replace(m_tlpFmt_, std::regex("\\{" + kTZPlaceholder + "\\}"), tzText_);
      m_tlpText_ = std::regex_replace(
          m_tlpText_, std::regex("\\{" + kCldPlaceholder + "\\}"),
          fmt_lib::vformat(m_locale_, cldText_, fmt_lib::make_format_args(shiftedNow)));
      m_tlpText_ =
          std::regex_replace(m_tlpText_, std::regex("\\{" + kOrdPlaceholder + "\\}"), ordText_);
    } else {
      m_tlpText_ = m_tlpFmt_;
    }

    m_tlpText_ = fmt_lib::vformat(m_locale_, m_tlpText_, fmt_lib::make_format_args(now));
    m_tooltip_->set_markup(m_tlpText_);
    label_.trigger_tooltip_query();
  }

  ALabel::update();
}

auto waybar::modules::Clock::getTZtext(sys_seconds now) -> std::string {
  if (tzList_.size() == 1) return "";

  std::stringstream os;
  for (size_t tz_idx{0}; tz_idx < tzList_.size(); ++tz_idx) {
    if (static_cast<int>(tz_idx) == tzCurrIdx_) continue;
    const auto* tz = tzList_[tz_idx] != nullptr ? tzList_[tz_idx] : local_zone();
    auto zt{zoned_time{tz, now}};
    os << fmt_lib::vformat(m_locale_, format_, fmt_lib::make_format_args(zt)) << '\n';
  }

  return os.str();
}

const unsigned cldRowsInMonth(const year_month& ym, const weekday& firstdow) {
  return 2u + ceil<weeks>((weekday{ym / 1} - firstdow) + ((ym / last).day() - day{0})).count();
}

auto cldGetWeekForLine(const year_month& ym, const weekday& firstdow, const unsigned line)
    -> const year_month_weekday {
  unsigned index{line - 2};
  if (weekday{ym / 1} == firstdow) ++index;
  return ym / firstdow[index];
}

auto getCalendarLine(const year_month_day& currDate, const year_month ym, const unsigned line,
                     const weekday& firstdow, const std::locale* const m_locale_) -> std::string {
  std::ostringstream os;

  switch (line) {
    // Print month and year title
    case 0: {
      os << date::format(*m_locale_, "{:L%B %Y}", ym);
      break;
    }
    // Print weekday names title
    case 1: {
      auto wd{firstdow};
      Glib::ustring wdStr;
      Glib::ustring::size_type wdLen{0};
      int clen{0};
      do {
        wdStr = date::format(*m_locale_, "{:L%a}", wd);
        clen = ustring_clen(wdStr);
        wdLen = wdStr.length();
        while (clen > 2) {
          wdStr = wdStr.substr(0, wdLen - 1);
          --wdLen;
          clen = ustring_clen(wdStr);
        }
        const std::string pad(2 - clen, ' ');

        if (wd != firstdow) os << ' ';

        os << pad << wdStr;
      } while (++wd != firstdow);
      break;
    }
    // Print first week prefixed with spaces if necessary
    case 2: {
      auto d{day{1}};
      auto wd{weekday{ym / 1}};
      os << std::string((wd - firstdow).count() * 3, ' ');

      if (currDate != ym / d)
        os << date::format(*m_locale_, "{:L%e}", d);
      else
        os << "{today}";

      while (++wd != firstdow) {
        ++d;

        if (currDate != ym / d)
          os << date::format(*m_locale_, " {:L%e}", d);
        else
          os << " {today}";
      }
      break;
    }
    // Print non-first week
    default: {
      auto ymdTmp{cldGetWeekForLine(ym, firstdow, line)};
      if (ymdTmp.ok()) {
        auto d{year_month_day{ymdTmp}.day()};
        const auto dlast{(ym / last).day()};
        auto wd{firstdow};

        if (currDate != ym / d)
          os << date::format(*m_locale_, "{:L%e}", d);
        else
          os << "{today}";

        while (++wd != firstdow && ++d <= dlast) {
          if (currDate != ym / d)
            os << date::format(*m_locale_, " {:L%e}", d);
          else
            os << " {today}";
        }
        // Append row with spaces if the week was not completed
        os << std::string((firstdow - wd).count() * 3, ' ');
      }
      break;
    }
  }

  return os.str();
}

auto waybar::modules::Clock::get_calendar(const year_month_day& today, const year_month_day& ymd,
                                          const time_zone* tz) -> const std::string {
  const auto firstdow{first_day_of_week()};
  const auto maxRows{12 / cldMonCols_};
  const auto ym{ymd.year() / ymd.month()};
  const auto y{ymd.year()};
  const auto d{ymd.day()};

  std::ostringstream os;
  std::ostringstream tmp;

  if (cldMode_ == CldMode::YEAR) {
    if (y / month{1} / 1 == cldYearShift_)
      if (d == cldBaseDay_ || (uint)cldBaseDay_ == 0u)
        return cldYearCached_;
      else
        cldBaseDay_ = d;
    else
      cldYearShift_ = y / month{1} / 1;
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
  // Pad object
  const std::string pads(cldWnLen_, ' ');
  // Compute number of lines needed for each calendar month
  unsigned ml[12]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

  for (auto& m : ml) {
    if (cldMode_ == CldMode::YEAR || m == static_cast<unsigned>(ymd.month()))
      m = cldRowsInMonth(y / month{m}, firstdow);
    else
      m = 0u;
  }

  for (auto row{0u}; row < maxRows; ++row) {
    const auto lines{*std::max_element(std::begin(ml) + (row * cldMonCols_),
                                       std::begin(ml) + ((row + 1) * cldMonCols_))};
    for (auto line{0u}; line < lines; ++line) {
      for (auto col{0u}; col < cldMonCols_; ++col) {
        const auto mon{month{row * cldMonCols_ + col + 1}};
        if (cldMode_ == CldMode::YEAR || y / mon == ym) {
          const year_month ymTmp{y / mon};
          if (col != 0 && cldMode_ == CldMode::YEAR) os << std::string(3, ' ');

          // Week numbers on the left
          if (cldWPos_ == WS::LEFT && line > 0) {
            if (line > 1) {
              if (line < ml[(unsigned)ymTmp.month() - 1u]) {
                os << fmt_lib::vformat(
                          m_locale_, fmtMap_[4],
                          fmt_lib::make_format_args(
                              (line == 2)
                                  ? static_cast<const zoned_seconds&&>(
                                        zoned_seconds{tz, local_days{ymTmp / 1}})
                                  : static_cast<const zoned_seconds&&>(zoned_seconds{
                                        tz, local_days{cldGetWeekForLine(ymTmp, firstdow, line)}})))
                   << ' ';
              } else
                os << pads;
            }
          }

          // Count wide characters to avoid extra padding
          size_t wideCharCount = 0;
          std::string calendarLine = getCalendarLine(today, ymTmp, line, firstdow, &m_locale_);
          if (line < 2) {
            for (gchar *data = calendarLine.data(), *end = data + calendarLine.size();
                 data != nullptr;) {
              gunichar c = g_utf8_get_char_validated(data, end - data);
              if (g_unichar_iswide(c)) {
                wideCharCount++;
              }
              data = g_utf8_find_next_char(data, end);
            }
          }
          os << Glib::ustring::format(
              (cldWPos_ != WS::LEFT || line == 0) ? std::left : std::right, std::setfill(L' '),
              std::setw(cldMonColLen_ + ((line < 2) ? cldWnLen_ - wideCharCount : 0)),
              calendarLine);

          // Week numbers on the right
          if (cldWPos_ == WS::RIGHT && line > 0) {
            if (line > 1) {
              if (line < ml[(unsigned)ymTmp.month() - 1u])
                os << ' '
                   << fmt_lib::vformat(
                          m_locale_, fmtMap_[4],
                          fmt_lib::make_format_args(
                              (line == 2) ? static_cast<const zoned_seconds&&>(
                                                zoned_seconds{tz, local_days{ymTmp / 1}})
                                          : static_cast<const zoned_seconds&&>(
                                                zoned_seconds{tz, local_days{cldGetWeekForLine(
                                                                      ymTmp, firstdow, line)}})));
              else
                os << pads;
            }
          }
        }
      }
      // Apply user's formats
      if (line < 2)
        tmp << fmt_lib::vformat(
            m_locale_, fmtMap_[line],
            fmt_lib::make_format_args(static_cast<const std::string_view&&>(os.str())));
      else
        tmp << os.str();
      // Clear ostringstream
      std::ostringstream().swap(os);
      if (line + 1u != lines || (row + 1u != maxRows && cldMode_ == CldMode::YEAR)) tmp << '\n';
    }
    if (row + 1u != maxRows && cldMode_ == CldMode::YEAR) tmp << '\n';
  }

  os << std::regex_replace(
      fmt_lib::vformat(m_locale_, fmtMap_[2],
                       fmt_lib::make_format_args(static_cast<const std::string_view&&>(tmp.str()))),
      std::regex("\\{today\\}"),
      fmt_lib::vformat(m_locale_, fmtMap_[3],
                       fmt_lib::make_format_args(
                           static_cast<const std::string_view&&>(date::format("{:L%e}", d)))));

  if (cldMode_ == CldMode::YEAR)
    cldYearCached_ = os.str();
  else
    cldMonCached_ = os.str();

  return os.str();
}

auto waybar::modules::Clock::local_zone() -> const time_zone* {
  const char* tz_name = getenv("TZ");
  if (tz_name) {
    try {
      return locate_zone(tz_name);
    } catch (const std::runtime_error& e) {
      spdlog::warn("Timezone: {0}. {1}", tz_name, e.what());
    }
  }
  return current_zone();
}

// Actions handler
auto waybar::modules::Clock::doAction(const std::string& name) -> void {
  if (actionMap_[name]) {
    (this->*actionMap_[name])();
  } else
    spdlog::error("Clock. Unsupported action \"{0}\"", name);
}

// Module actions
void waybar::modules::Clock::cldModeSwitch() {
  cldMode_ = (cldMode_ == CldMode::YEAR) ? CldMode::MONTH : CldMode::YEAR;
}
void waybar::modules::Clock::cldShift_up() {
  cldCurrShift_ += (months)((cldMode_ == CldMode::YEAR) ? 12 : 1) * cldShift_;
}
void waybar::modules::Clock::cldShift_down() {
  cldCurrShift_ -= (months)((cldMode_ == CldMode::YEAR) ? 12 : 1) * cldShift_;
}
void waybar::modules::Clock::cldShift_reset() { cldCurrShift_ = (months)0; }
void waybar::modules::Clock::tz_up() {
  const auto tzSize{tzList_.size()};
  if (tzSize == 1) return;
  size_t newIdx{tzCurrIdx_ + 1lu};
  tzCurrIdx_ = (newIdx == tzSize) ? 0 : newIdx;
}
void waybar::modules::Clock::tz_down() {
  const auto tzSize{tzList_.size()};
  if (tzSize == 1) return;
  tzCurrIdx_ = (tzCurrIdx_ == 0) ? tzSize - 1 : tzCurrIdx_ - 1;
}

#ifdef HAVE_LANGINFO_1STDAY
template <auto fn>
using deleter_from_fn = std::integral_constant<decltype(fn), fn>;

template <typename T, auto fn>
using deleting_unique_ptr = std::unique_ptr<T, deleter_from_fn<fn>>;
#endif

// Computations done similarly to Linux cal utility.
auto waybar::modules::Clock::first_day_of_week() -> weekday {
#ifdef HAVE_LANGINFO_1STDAY
  deleting_unique_ptr<std::remove_pointer<locale_t>::type, freelocale> posix_locale{
      newlocale(LC_ALL, m_locale_.name().c_str(), nullptr)};
  if (posix_locale) {
    const auto i{(int)((std::intptr_t)nl_langinfo_l(_NL_TIME_WEEK_1STDAY, posix_locale.get()))};
    const weekday wd{year_month_day{year(i / 10000) / month(i / 100 % 100) / day(i % 100)}};
    const auto j{(uint8_t)*nl_langinfo_l(_NL_TIME_FIRST_WEEKDAY, posix_locale.get())};
    return wd + days{j - 1};
  }
#endif
  return Sunday;
}

auto waybar::modules::Clock::get_ordinal_date(const year_month_day& today) -> std::string {
  auto day = static_cast<unsigned int>(today.day());
  std::stringstream res;
  res << day;
  if (day >= 11 && day <= 13) {
    res << "th";
    return res.str();
  }

  switch (day % 10) {
    case 1:
      res << "st";
      break;
    case 2:
      res << "nd";
      break;
    case 3:
      res << "rd";
      break;
    default:
      res << "th";
  }
  return res.str();
}
