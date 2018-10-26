#include "modules/memory.hpp"

waybar::modules::Memory::Memory(const Json::Value& config)
  : ALabel(config, "{}%")
{
  label_.set_name("memory");
  uint32_t interval = config_["interval"].isUInt() ? config_["interval"].asUInt() : 30;
  thread_ = [this, interval] {
    dp.emit();
    thread_.sleep_for(chrono::seconds(interval));
  };
}

auto waybar::modules::Memory::update() -> void
{
  struct sysinfo info = {};
  if (sysinfo(&info) == 0) {
    auto total = info.totalram * info.mem_unit;
    auto freeram = info.freeram * info.mem_unit;
    int used_ram_percentage = 100 * (total - freeram) / total;
    label_.set_text(fmt::format(format_, used_ram_percentage));
    auto used_ram_gigabytes = (total - freeram) / std::pow(1024, 3);
    label_.set_tooltip_text(fmt::format("{:.{}f}Gb used", used_ram_gigabytes, 1));
  }
}
