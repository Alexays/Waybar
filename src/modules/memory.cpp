#include "modules/memory.hpp"

waybar::modules::Memory::Memory(Json::Value config)
  : _config(config)
{
  _label.get_style_context()->add_class("memory");
  int interval = _config["interval"] ? _config["inveral"].asInt() : 30;
  _thread = [this, interval] {
    Glib::signal_idle().connect_once([this] {
      update();
    });
    _thread.sleep_for(chrono::seconds(interval));
  };
};

auto waybar::modules::Memory::update() -> void
{
  struct sysinfo info;
  if (!sysinfo(&info)) {
    auto total = info.totalram * info.mem_unit;
    auto freeram = info.freeram * info.mem_unit;
    int used_ram_percentage = 100 * (total - freeram) / total;
    auto format = _config["format"] ? _config["format"].asString() : "{}%";
    _label.set_text(fmt::format(format, used_ram_percentage));
    auto used_ram_gigabytes = (total - freeram) / std::pow(1024, 3);
    _label.set_tooltip_text(fmt::format("{:.{}f}Gb used", used_ram_gigabytes, 1));
  }
}

waybar::modules::Memory::operator Gtk::Widget &() {
  return _label;
}
