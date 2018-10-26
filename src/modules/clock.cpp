#include "modules/clock.hpp"

waybar::modules::Clock::Clock(const Json::Value& config)
  : ALabel(config, "{:%H:%M}")
{
  label_.set_name("clock");
  uint32_t interval = config_["interval"].isUInt() ? config_["interval"].asUInt() : 60;
  thread_ = [this, interval] {
    auto now = waybar::chrono::clock::now();
    dp.emit();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now
      + std::chrono::seconds(interval));
    thread_.sleep_until(timeout);
  };
}

auto waybar::modules::Clock::update() -> void
{
  auto localtime = fmt::localtime(std::time(nullptr));
  label_.set_text(fmt::format(format_, localtime));
}
