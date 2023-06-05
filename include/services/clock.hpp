#pragma once

#include "services/DBusService.hpp"
#include "util/date.hpp"

namespace waybar::services {

const std::string kCalendarPlaceholder = "calendar";
const std::string KTimezonedTimeListPlaceholder = "timezoned_time_list";

enum class WeeksSide {
  LEFT,
  RIGHT,
  HIDDEN,
};

enum class CldMode { MONTH, YEAR };

class Clock final : public DBusService {
 public:
  Clock(const std::string&, const Json::Value&);
  virtual ~Clock() = default;
  auto doAction(const Glib::ustring &name) -> void override;
  bool doActionExists(const Glib::ustring &name) override;
  auto start() -> void override;
  auto stop() -> void override;
 private:
  std::locale locale_;
  std::vector<const date::time_zone*> time_zones_;
  int current_time_zone_idx_;
  bool is_calendar_in_tooltip_;
  bool is_timezoned_list_in_tooltip_;

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
  date::day cldBaseDay_{0};
  /*Calendar functions*/
  auto get_calendar(const date::zoned_seconds& now, const date::zoned_seconds& wtime)
      -> std::string;
  /*Clock actions*/
  void cldModeSwitch();
  void cldShift_up();
  void cldShift_down();
  void tz_up();
  void tz_down();

  const Glib::ustring& getLabelText() override;
  const Glib::ustring& getTooltipText() override;

  // ModuleActionMap
  static inline std::map<const std::string, void (waybar::services::Clock::*const)()> actionMap_{
      {"mode", &waybar::services::Clock::cldModeSwitch},
      {"shift_up", &waybar::services::Clock::cldShift_up},
      {"shift_down", &waybar::services::Clock::cldShift_down},
      {"tz_up", &waybar::services::Clock::tz_up},
      {"tz_down", &waybar::services::Clock::tz_down}};

};  // namespace waybar::services
}
