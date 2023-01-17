#pragma once

#include "ALabel.hpp"
#include "util/date.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

const std::string kCalendarPlaceholder = "calendar";
const std::string KTimezonedTimeListPlaceholder = "timezoned_time_list";

class Clock : public ALabel {
 public:
  Clock(const std::string&, const Json::Value&);
  ~Clock() = default;
  auto update() -> void;

 private:
  util::SleeperThread thread_;
  std::locale locale_;
  std::vector<const date::time_zone*> time_zones_;
  int current_time_zone_idx_;
  date::year_month_day calendar_cached_ymd_{date::January / 1 / 0};
  date::months calendar_shift_{0}, calendar_shift_init_{0};
  std::string calendar_cached_text_;
  bool is_calendar_in_tooltip_;
  bool is_timezoned_list_in_tooltip_;

  bool handleScroll(GdkEventScroll* e);

  std::string fmt_str_weeks_;
  std::string fmt_str_calendar_;
  int fmt_weeks_left_pad_{0};
  auto calendar_text(const date::zoned_seconds& ztime) -> std::string;
  auto weekdays_header(const date::weekday& first_dow, std::ostream& os) -> void;
  auto first_day_of_week() -> date::weekday;
  const date::time_zone* current_timezone();
  bool is_timezone_fixed();
  auto timezones_text(std::chrono::system_clock::time_point* now) -> std::string;
};
}  // namespace waybar::modules
