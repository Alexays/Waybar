#include "modules/clock.hpp"
#include <time.h>

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60) {
  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now + interval_);
    auto diff = std::chrono::seconds(timeout.time_since_epoch().count() % interval_.count());
    thread_.sleep_until(timeout - diff);
  };
}

auto waybar::modules::Clock::update() -> void {
  tzset(); // Update timezone information
  auto now = std::chrono::system_clock::now();
  auto localtime = fmt::localtime(std::chrono::system_clock::to_time_t(now));
  auto text = fmt::format(format_, localtime);
  label_.set_markup(text);

  if (tooltipEnabled()) {
    if (config_["tooltip-format"].isString()) {
      auto tooltip_format = config_["tooltip-format"].asString();
      auto tooltip_text = fmt::format(tooltip_format, localtime);
      label_.set_tooltip_text(tooltip_text);
    } else {
      label_.set_tooltip_text(text);
    }
  }
}
