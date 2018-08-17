#include "modules/memory.hpp"

waybar::modules::Memory::Memory(Json::Value config)
  : config_(std::move(config))
{
  label_.set_name("memory");
  uint32_t interval = config_["interval"] ? config_["inveral"].asUInt() : 30;
  thread_ = [this, interval] {
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Memory::update));
    thread_.sleep_for(chrono::seconds(interval));
  };
};

auto waybar::modules::Memory::update() -> void
{
  struct sysinfo info = {0};
  if (sysinfo(&info) == 0) {
    auto total = info.totalram * info.mem_unit;
    auto freeram = info.freeram * info.mem_unit;
    int used_ram_percentage = 100 * (total - freeram) / total;
    auto format = config_["format"] ? config_["format"].asString() : "{}%";
    label_.set_text(fmt::format(format, used_ram_percentage));
    auto used_ram_gigabytes = (total - freeram) / std::pow(1024, 3);
    label_.set_tooltip_text(fmt::format("{:.{}f}Gb used", used_ram_gigabytes, 1));
  }
}

waybar::modules::Memory::operator Gtk::Widget &() {
  return label_;
}
