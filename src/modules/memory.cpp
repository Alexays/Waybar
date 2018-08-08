#include "modules/memory.hpp"
#include <iostream>

waybar::modules::Memory::Memory()
{
  _label.get_style_context()->add_class("memory-widget");
  _thread = [this] {
    struct sysinfo info;
    if (!sysinfo(&info)) {
      double available = (double)info.freeram / (double)info.totalram;
      std::cout << available << std::endl;
      _label.set_text(fmt::format("{:.{}f}% ", available * 100, 1));
    }
    _thread.sleep_for(chrono::seconds(30));
  };
};

waybar::modules::Memory::operator Gtk::Widget &() {
  return _label;
}
