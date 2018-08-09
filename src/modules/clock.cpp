#include "modules/clock.hpp"

waybar::modules::Clock::Clock(Json::Value config)
  : _config(config)
{
  _label.get_style_context()->add_class("clock");
  _thread = [this] {
    update();
    auto now = waybar::chrono::clock::now();
    auto timeout =
      std::chrono::floor<std::chrono::minutes>(now + std::chrono::minutes(1));
    _thread.sleep_until(timeout);
  };
};

auto waybar::modules::Clock::update() -> void
{
  auto t = std::time(nullptr);
  auto localtime = std::localtime(&t);
  auto format =
    _config["format"] ? _config["format"].asString() : "{:02}:{:02}";
  _label.set_text(fmt::format(format, localtime->tm_hour, localtime->tm_min));
}

waybar::modules::Clock::operator Gtk::Widget &() {
  return _label;
}
