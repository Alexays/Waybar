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

class Clock : public ALabel {
 public:
  Clock(const std::string&, const Json::Value&);
  ~Clock() = default;
  auto update() -> void;

 private:
  util::SleeperThread thread_;
  std::locale locale_;
  const date::time_zone* time_zone_;
  bool fixed_time_zone_;
  int time_zone_idx_;
  date::year_month_day cached_calendar_ymd_;
  std::string cached_calendar_text_;

  bool handleScroll(GdkEventScroll* e);

  auto calendar_text(const waybar_time& wtime) -> std::string;
  auto weekdays_header(const date::weekday& first_dow, std::ostream& os) -> void;
  auto first_day_of_week() -> date::weekday;
};

}  // namespace waybar::modules
