#include "modules/cpu.hpp"
#include <iostream>

waybar::modules::Cpu::Cpu()
{
  _label.get_style_context()->add_class("cpu-widget");
  _thread = [this] {
    struct sysinfo info;
    if (!sysinfo(&info)) {
      float f_load = 1.f / (1 << SI_LOAD_SHIFT);
      _label.set_text(fmt::format("{:.{}f}% ",
      	info.loads[0] * f_load * 100 / get_nprocs(), 1));
    }
    _thread.sleep_for(chrono::seconds(30));
  };
};

waybar::modules::Cpu::operator Gtk::Widget &() {
  return _label;
}
