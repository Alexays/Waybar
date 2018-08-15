#include "modules/clock.hpp"

waybar::modules::Clock::Clock(Json::Value config)
  : _config(config)
{
  _label.set_name("clock");
  _thread = [this] {
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Clock::update));
    auto now = waybar::chrono::clock::now();
    auto timeout =
      std::chrono::floor<std::chrono::minutes>(now + std::chrono::minutes(1));
    _thread.sleep_until(timeout);
  };
};

auto waybar::modules::Clock::update() -> void
{
  auto localtime = fmt::localtime(std::time(nullptr));
  auto format =
    _config["format"] ? _config["format"].asString() : "{:%H:%M}";
  _label.set_text(fmt::format(format, localtime));
}

waybar::modules::Clock::operator Gtk::Widget &() {
  return _label;
}
