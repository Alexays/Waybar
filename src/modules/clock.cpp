#include "modules/clock.hpp"

waybar::modules::Clock::Clock(Json::Value config)
  : _config(config)
{
  _label.set_name("clock");
  int interval = _config["interval"] ? _config["inveral"].asInt() : 60;
  _thread = [this, interval] {
    auto now = waybar::chrono::clock::now();
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Clock::update));
    auto timeout = std::chrono::floor<std::chrono::seconds>(now
      + std::chrono::seconds(interval));
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
