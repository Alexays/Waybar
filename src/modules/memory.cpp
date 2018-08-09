#include "modules/memory.hpp"
#include <iostream>

waybar::modules::Memory::Memory(Json::Value config)
  : _config(config)
{
  _label.get_style_context()->add_class("memory");
  _thread = [this] {
    update();
    int interval = _config["interval"] ? _config["inveral"].asInt() : 30;
    _thread.sleep_for(chrono::seconds(interval));
  };
};

auto waybar::modules::Memory::update() -> void
{
  struct sysinfo info;
  if (!sysinfo(&info)) {
    int available = 100 - ((double)info.freeram / (double)info.totalram) * 100;
    auto format = _config["format"] ? _config["format"].asString() : "{}%";
    _label.set_text(fmt::format(format, available));
    auto used = (info.totalram - info.freeram) / std::pow(1024, 3);
    _label.set_tooltip_text(fmt::format("{:.{}f}Gb used", used, 1));
  }
}

waybar::modules::Memory::operator Gtk::Widget &() {
  return _label;
}
