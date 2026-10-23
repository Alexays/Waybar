#include "modules/simpleclock.hpp"

#include <time.h>

namespace waybar::modules {

Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60) {
  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    /* difference with projected wakeup time */
    auto diff = now.time_since_epoch() % interval_;
    /* sleep until the next projected time */
    thread_.sleep_for(interval_ - diff);
  };
}

auto Clock::doUpdate() -> void {
  tzset();  // Update timezone information
  auto now{std::chrono::system_clock::now()};
  auto time{std::chrono::system_clock::to_time_t(now)};
  std::tm localtime;
  localtime_r(&time, &localtime);
  updateLabelAndTooltip(format_, format_, localtime);
  // Call parent update
  ALabel::doUpdate();
}

} /* namespace waybar::modules */
