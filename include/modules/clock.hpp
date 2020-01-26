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
};

}  // namespace waybar::modules
