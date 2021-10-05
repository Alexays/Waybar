#pragma once

#include <fmt/format.h>
#if FMT_VERSION < 60000
#include <fmt/time.h>
#else
#include <fmt/chrono.h>
#endif
#include <date/tz.h>
#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

struct waybar_time {
  std::locale locale;
  date::zoned_seconds ztime;
};

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
  date::year_month_day cached_calendar_ymd_ = date::January/1/0;
  std::string cached_calendar_text_;
  bool is_calendar_in_tooltip_;
  bool is_timezoned_list_in_tooltip_;

  bool handleScroll(GdkEventScroll* e);

  auto calendar_text(const waybar_time& wtime) -> std::string;
  auto weekdays_header(const date::weekday& first_dow, std::ostream& os) -> void;
  auto first_day_of_week() -> date::weekday;
  const date::time_zone* current_timezone();
  bool is_timezone_fixed();
  auto timezones_text(std::chrono::_V2::system_clock::time_point *now) -> std::string;
};

}  // namespace waybar::modules
