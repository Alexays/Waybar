#include "modules/clock.hpp"

waybar::modules::Clock::Clock(Json::Value config)
  : ALabel(std::move(config))
{
  label_.set_name("clock");
  uint32_t interval = config_["interval"] ? config_["inveral"].asUInt() : 60;
  thread_.sig_update.connect(sigc::mem_fun(*this, &Clock::update));
  thread_ = [this, interval] {
    auto now = waybar::chrono::clock::now();
    thread_.emit();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now
      + std::chrono::seconds(interval));
    thread_.sleep_until(timeout);
  };
}

auto waybar::modules::Clock::update() -> void
{
  auto localtime = fmt::localtime(std::time(nullptr));
  auto format = config_["format"] ? config_["format"].asString() : "{:%H:%M}";
  label_.set_text(fmt::format(format, localtime));
}
