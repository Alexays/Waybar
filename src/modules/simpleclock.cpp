#define _POSIX_C_SOURCE 202405L

#include "modules/simpleclock.hpp"

#include <errno.h>
#include <string.h>
#include <time.h>

#include <exception>
#include <string>

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
  time_t now = time(NULL);
  if (now == (time_t)-1)
    throw std::runtime_error("Failed to get current time: " + std::string(strerror(errno)));
  struct tm localtime;
  if (!localtime_r(&now, &localtime))
    throw std::runtime_error("Failed to convert time with localtime_r(): " +
                             std::string(strerror(errno)));

  updateLabelAndTooltip(format_, format_, localtime);
  // Call parent update
  ALabel::update();
}
