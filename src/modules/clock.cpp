#include "modules/clock.hpp"

waybar::modules::Clock::Clock()
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
  _label.set_text(
      fmt::format("{:02}:{:02}", localtime->tm_hour, localtime->tm_min));
}

waybar::modules::Clock::operator Gtk::Widget &() {
  return _label;
}
