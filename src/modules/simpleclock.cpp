#include "modules/simpleclock.hpp"

#include <time.h>

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
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

auto waybar::modules::Clock::update() -> void {
  tzset();  // Update timezone information
  auto now = std::chrono::system_clock::now();
  auto localtime = fmt::localtime(std::chrono::system_clock::to_time_t(now));
  updateLabelAndTooltip(format_, format_, localtime);
  // Call parent update
  ALabel::update();
}
