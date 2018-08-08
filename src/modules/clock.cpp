#include "modules/clock.hpp"

waybar::modules::Clock::Clock()
{
  _label.get_style_context()->add_class("clock-widget");
  _thread = [this] {
    auto now = waybar::chrono::clock::now();
    auto t = std::time(nullptr);
    auto localtime = std::localtime(&t);
    _label.set_text(
        fmt::format("{:02}:{:02}", localtime->tm_hour, localtime->tm_min));
    auto timeout =
      std::chrono::floor<std::chrono::minutes>(now + std::chrono::minutes(1));
    _thread.sleep_until(timeout);
  };
};

waybar::modules::Clock::operator Gtk::Widget &() {
  return _label;
}
