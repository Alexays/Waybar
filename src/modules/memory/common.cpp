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
  unsigned long swaptotal = 0;
  if (meminfo_.count("SwapTotal")) {
    swaptotal = meminfo_["SwapTotal"];
  }
  unsigned long memfree;
  unsigned long swapfree = 0;
  if (meminfo_.count("SwapFree")) {
    swapfree = meminfo_["SwapFree"];
  }
  if (meminfo_.count("MemAvailable")) {
    // New kernels (3.4+) have an accurate available memory field.
    memfree = meminfo_["MemAvailable"] + meminfo_["zfs_size"];
  } else {
    // Old kernel; give a best-effort approximation of available memory.
    memfree = meminfo_["MemFree"] + meminfo_["Buffers"] + meminfo_["Cached"] +
              meminfo_["SReclaimable"] - meminfo_["Shmem"] + meminfo_["zfs_size"];
  }

  if (memtotal > 0 && memfree >= 0) {
    float total_ram_gigabytes =
        0.01 * round(memtotal / 10485.76);  // 100*10485.76 = 2^20 = 1024^2 = GiB/KiB
    float total_swap_gigabytes = 0.01 * round(swaptotal / 10485.76);
    int used_ram_percentage = 100 * (memtotal - memfree) / memtotal;
    int used_swap_percentage = 0;
    if (swaptotal && swapfree) {
      used_swap_percentage = 100 * (swaptotal - swapfree) / swaptotal;
    }
    float used_ram_gigabytes = 0.01 * round((memtotal - memfree) / 10485.76);
    float used_swap_gigabytes = 0.01 * round((swaptotal - swapfree) / 10485.76);
    float available_ram_gigabytes = 0.01 * round(memfree / 10485.76);
    float available_swap_gigabytes = 0.01 * round(swapfree / 10485.76);

    auto format = format_;
    auto state = getState(used_ram_percentage);
    if (!state.empty() && config_["format-" + state].isString()) {
      format = config_["format-" + state].asString();
    }

    if (format.empty()) {
      event_box_.hide();
    } else {
      event_box_.show();
      auto icons = std::vector<std::string>{state};
      label_.set_markup(fmt::format(
          fmt::runtime(format), used_ram_percentage,
          fmt::arg("icon", getIcon(used_ram_percentage, icons)),
          fmt::arg("total", total_ram_gigabytes), fmt::arg("swapTotal", total_swap_gigabytes),
          fmt::arg("percentage", used_ram_percentage),
          fmt::arg("swapState", swaptotal == 0 ? "Off" : "On"),
          fmt::arg("swapPercentage", used_swap_percentage), fmt::arg("used", used_ram_gigabytes),
          fmt::arg("swapUsed", used_swap_gigabytes), fmt::arg("avail", available_ram_gigabytes),
          fmt::arg("swapAvail", available_swap_gigabytes)));
    }

    if (tooltipEnabled()) {
      if (config_["tooltip-format"].isString()) {
        auto tooltip_format = config_["tooltip-format"].asString();
        label_.set_tooltip_text(fmt::format(
            fmt::runtime(tooltip_format), used_ram_percentage,
            fmt::arg("total", total_ram_gigabytes), fmt::arg("swapTotal", total_swap_gigabytes),
            fmt::arg("percentage", used_ram_percentage),
            fmt::arg("swapState", swaptotal == 0 ? "Off" : "On"),
            fmt::arg("swapPercentage", used_swap_percentage), fmt::arg("used", used_ram_gigabytes),
            fmt::arg("swapUsed", used_swap_gigabytes), fmt::arg("avail", available_ram_gigabytes),
            fmt::arg("swapAvail", available_swap_gigabytes)));
      } else {
        label_.set_tooltip_text(fmt::format("{:.{}f}GiB used", used_ram_gigabytes, 1));
      }
    }
  } else {
    event_box_.hide();
  }
  // Call parent update
  ALabel::update();
}
