#include "modules/cpu.hpp"

waybar::modules::Cpu::Cpu(Json::Value config)
  : _config(std::move(config))
{
  _label.set_name("cpu");
  int interval = _config["interval"] ? _config["inveral"].asInt() : 10;
  _thread = [this, interval] {
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Cpu::update));
    _thread.sleep_for(chrono::seconds(interval));
  };
};

auto waybar::modules::Cpu::update() -> void
{
  struct sysinfo info = {};
  if (sysinfo(&info) == 0) {
    float f_load = 1.f / (1U << SI_LOAD_SHIFT);
    int load = info.loads[0] * f_load * 100 / get_nprocs();
    auto format = _config["format"] ? _config["format"].asString() : "{}%";
    _label.set_text(fmt::format(format, load));
  }
}

waybar::modules::Cpu::operator Gtk::Widget &() {
  return _label;
}
