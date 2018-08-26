#include "modules/cpu.hpp"

waybar::modules::Cpu::Cpu(const Json::Value& config)
  : ALabel(config, "{}%")
{
  label_.set_name("cpu");
  uint32_t interval = config_["interval"] ? config_["inveral"].asUInt() : 10;
  thread_ = [this, interval] {
    dp.emit();
    thread_.sleep_for(chrono::seconds(interval));
  };
}

auto waybar::modules::Cpu::update() -> void
{
  struct sysinfo info = {0};
  if (sysinfo(&info) == 0) {
    float f_load = 1.f / (1u << SI_LOAD_SHIFT);
    uint16_t load = info.loads[0] * f_load * 100 / get_nprocs();
    label_.set_text(fmt::format(format_, load));
  }
}
