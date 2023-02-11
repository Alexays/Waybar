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

class Clock : public ALabel {
 public:
  Clock(const std::string&, const Json::Value&);
  ~Clock() = default;
  auto update() -> void;

 private:
  util::SleeperThread thread_;
  std::map<std::pair<uint, GdkEventType>, void (waybar::modules::Clock::*)()> eventMap_;
  std::locale locale_;
  std::vector<const date::time_zone*> time_zones_;
  int current_time_zone_idx_;
  bool is_calendar_in_tooltip_;
  bool is_timezoned_list_in_tooltip_;

  bool handleScroll(GdkEventScroll* e);
  bool handleToggle(GdkEventButton* const& e);

  auto first_day_of_week() -> date::weekday;
  const date::time_zone* current_timezone();
  bool is_timezone_fixed();
  auto timezones_text(std::chrono::system_clock::time_point* now) -> std::string;

  /*Calendar properties*/
  WeeksSide cldWPos_{WeeksSide::HIDDEN};
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
  /*Calendar functions*/
  auto get_calendar(const date::zoned_seconds& now, const date::zoned_seconds& wtime)
      -> std::string;
  void cldModeSwitch();
};
}  // namespace waybar::modules
