#include "modules/cpu.hpp"

waybar::modules::Cpu::Cpu(Json::Value config)
  : _config(config)
{
  _label.get_style_context()->add_class("cpu");
  _thread = [this] {
    update();
    int interval = _config["interval"] ? _config["inveral"].asInt() : 10;
    _thread.sleep_for(chrono::seconds(interval));
  };
};

auto waybar::modules::Cpu::update() -> void
{
  struct sysinfo info;
  if (!sysinfo(&info)) {
    float f_load = 1.f / (1 << SI_LOAD_SHIFT);
    int load = info.loads[0] * f_load * 100 / get_nprocs();
    auto format = _config["format"] ? _config["format"].asString() : "{}%";
    _label.set_text(fmt::format(format, load));
  }
}

waybar::modules::Cpu::operator Gtk::Widget &() {
  return _label;
}
