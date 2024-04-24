#pragma once

#include "ALabel.hpp"
#include "util/date.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

const std::string kCldPlaceholder{"calendar"};
const std::string kTZPlaceholder{"tz_list"};
const std::string kOrdPlaceholder{"ordinal_date"};

enum class CldMode { MONTH, YEAR };
enum class WS { LEFT, RIGHT, HIDDEN };

class Clock final : public ALabel {
 public:
  Clock(const std::string&, const Json::Value&);
  virtual ~Clock() = default;
  auto update() -> void override;
  auto doAction(const std::string&) -> void override;

 private:
  const std::locale locale_;
  // tooltip
  const std::string tlpFmt_;
  std::string tlpText_{""};  // tooltip text to print
  // Calendar
  const bool cldInTooltip_;  // calendar in tooltip
  /*
    0 - calendar.format.months
    1 - calendar.format.weekdays
    2 - calendar.format.days
    3 - calendar.format.today
    4 - calendar.format.weeks
    5 - tooltip-format
   */
  std::map<int, std::string const> fmtMap_;
  uint cldMonCols_{3};           // calendar count month columns
  int cldWnLen_{3};              // calendar week number length
  const int cldMonColLen_{20};   // calendar month column length
  WS cldWPos_{WS::HIDDEN};       // calendar week side to print
  months cldCurrShift_{0};       // calendar months shift
  int cldShift_{1};              // calendar months shift factor
  year_month_day cldYearShift_;  // calendar Year mode. Cached ymd
  std::string cldYearCached_;    // calendar Year mode. Cached calendar
  year_month cldMonShift_;       // calendar Month mode. Cached ym
  std::string cldMonCached_;     // calendar Month mode. Cached calendar
  day cldBaseDay_{0};            // calendar Cached day. Is used when today is changing(midnight)
  std::string cldText_{""};      // calendar text to print
  CldMode cldMode_{CldMode::MONTH};
  auto get_calendar(const year_month_day& today, const year_month_day& ymd, const time_zone* tz)
      -> const std::string;

  // get local time zone
  auto local_zone() -> const time_zone*;

  // time zoned time in tooltip
  const bool tzInTooltip_;                // if need to print time zones text
  std::vector<const time_zone*> tzList_;  // time zones list
  int tzCurrIdx_;                         // current time zone index for tzList_
  std::string tzText_{""};                // time zones text to print
  util::SleeperThread thread_;

  // ordinal date in tooltip
  const bool ordInTooltip_;
  std::string ordText_{""};
  auto get_ordinal_date(const year_month_day& today) -> std::string;

  auto getTZtext(sys_seconds now) -> std::string;
  auto first_day_of_week() -> weekday;
  // Module actions
  void cldModeSwitch();
  void cldShift_up();
  void cldShift_down();
  void cldShift_reset();
  void tz_up();
  void tz_down();
  // Module Action Map
  static inline std::map<const std::string, void (waybar::modules::Clock::*const)()> actionMap_{
      {"mode", &waybar::modules::Clock::cldModeSwitch},
      {"shift_up", &waybar::modules::Clock::cldShift_up},
      {"shift_down", &waybar::modules::Clock::cldShift_down},
      {"shift_reset", &waybar::modules::Clock::cldShift_reset},
      {"tz_up", &waybar::modules::Clock::tz_up},
      {"tz_down", &waybar::modules::Clock::tz_down}};
};

}  // namespace waybar::modules
