#include "modules/memory.hpp"

waybar::modules::Memory::Memory(const std::string& id, const Json::Value& config)
    : ALabel(config, "memory", id, "{}%", 30) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::Memory::update() -> void {
  parseMeminfo();

  unsigned long memtotal = meminfo_["MemTotal"];
  unsigned long memfree;
  if (meminfo_.count("MemAvailable")) {
    // New kernels (3.4+) have an accurate available memory field.
    memfree = meminfo_["MemAvailable"];
  } else {
    // Old kernel; give a best-effort approximation of available memory.
    memfree = meminfo_["MemFree"] + meminfo_["Buffers"] + meminfo_["Cached"] +
              meminfo_["SReclaimable"] - meminfo_["Shmem"];
  }

  if (memtotal > 0 && memfree >= 0) {
    auto total_ram_gigabytes = memtotal / std::pow(1024, 2);
    int  used_ram_percentage = 100 * (memtotal - memfree) / memtotal;
    auto used_ram_gigabytes = (memtotal - memfree) / std::pow(1024, 2);
    auto available_ram_gigabytes = memfree / std::pow(1024, 2);

    getState(used_ram_percentage);
    label_.set_markup(fmt::format(format_,
                                  used_ram_percentage,
                                  fmt::arg("total", total_ram_gigabytes),
                                  fmt::arg("percentage", used_ram_percentage),
                                  fmt::arg("used", used_ram_gigabytes),
                                  fmt::arg("avail", available_ram_gigabytes)));
    if (tooltipEnabled()) {
      label_.set_tooltip_text(fmt::format("{:.{}f}Gb used", used_ram_gigabytes, 1));
    }
    event_box_.show();
  } else {
    event_box_.hide();
  }
  // Call parent update
  ALabel::update();
}
