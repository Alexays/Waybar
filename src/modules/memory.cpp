#include "modules/memory.hpp"
#include <iostream>

waybar::modules::Memory::Memory()
{
  _label.get_style_context()->add_class("memory");
  _thread = [this] {
    struct sysinfo info;
    if (!sysinfo(&info)) {
      double available = (double)info.freeram / (double)info.totalram;
      _label.set_text(fmt::format("{:.{}f}% ", available * 100, 0));
    }
    _thread.sleep_for(chrono::seconds(30));
  };
};

waybar::modules::Memory::operator Gtk::Widget &() {
  return _label;
}
