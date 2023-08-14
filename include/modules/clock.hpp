#pragma once

#include "ALabel.hpp"
#include "util/date.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

const std::string kCalendarPlaceholder = "calendar";
const std::string KTimezonedTimeListPlaceholder = "timezoned_time_list";

enum class WeeksSide {
  LEFT,
  RIGHT,
  HIDDEN,
};

enum class CldMode { MONTH, YEAR };

class Clock final : public ALabel {
 public:
  Clock(const std::string&, const Json::Value&);
  virtual ~Clock() = default;
  auto update() -> void override;
  auto doAction(const std::string& name) -> void override;

 private:
  util::SleeperThread thread_;
  std::locale locale_;
  std::vector<const date::time_zone*> time_zones_;
  int current_time_zone_idx_;
  bool is_calendar_in_tooltip_;
  bool is_timezoned_list_in_tooltip_;

  auto first_day_of_week() -> date::weekday;
  const date::time_zone* current_timezone();
  auto timezones_text(std::chrono::system_clock::time_point now) -> std::string;

  /*Calendar properties*/
  WeeksSide cldWPos_{WeeksSide::HIDDEN};
  /*
    0 - calendar.format.months
    1 - calendar.format.weekdays
    2 - calendar.format.days
    3 - calendar.format.today
    4 - calendar.format.weeks
    5 - tooltip-format
   */
  std::map<int, std::string const> fmtMap_;
  CldMode cldMode_{CldMode::MONTH};
  uint cldMonCols_{3};    // Count of the month in the row
  int cldMonColLen_{20};  // Length of the month column
  int cldWnLen_{3};       // Length of the week number
  date::year_month_day cldYearShift_;
  date::year_month cldMonShift_;
  date::months cldCurrShift_{0};
  date::months cldShift_{0};
  std::string cldYearCached_{};
  std::string cldMonCached_{};
  date::day cldBaseDay_{0};
  /*Calendar functions*/
  auto get_calendar(const date::year_month_day& today, const date::year_month_day& ymd,
                    const date::time_zone* tz) -> const std::string;
  /*Clock actions*/
  void cldModeSwitch();
  void cldShift_up();
  void cldShift_down();
  void tz_up();
  void tz_down();

  // ModuleActionMap
  static inline std::map<const std::string, void (waybar::modules::Clock::*const)()> actionMap_{
      {"mode", &waybar::modules::Clock::cldModeSwitch},
      {"shift_up", &waybar::modules::Clock::cldShift_up},
      {"shift_down", &waybar::modules::Clock::cldShift_down},
      {"tz_up", &waybar::modules::Clock::tz_up},
      {"tz_down", &waybar::modules::Clock::tz_down}};
};
}  // namespace waybar::modules
