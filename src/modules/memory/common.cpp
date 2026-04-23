#include "modules/memory.hpp"

waybar::modules::Memory::Memory(const std::string& id, const Json::Value& config,
                                std::mutex& reap_mtx, std::list<pid_t>& reap)
    : ALabel(config, "memory", id, "{}%", reap_mtx, reap, 30) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
  if (config["unit"].isString()) {
    unit_ = config["unit"].asString();
  }
}

auto waybar::modules::Memory::update() -> void {
  parseMeminfo();

  unsigned long memtotal = meminfo_["MemTotal"];
  unsigned long swaptotal = 0;
  if (meminfo_.contains("SwapTotal")) {
    swaptotal = meminfo_["SwapTotal"];
  }
  unsigned long memfree;
  unsigned long swapfree = 0;
  if (meminfo_.contains("SwapFree")) {
    swapfree = meminfo_["SwapFree"];
  }
  if (meminfo_.contains("MemAvailable")) {
    // New kernels (3.4+) have an accurate available memory field.
    memfree = meminfo_["MemAvailable"] + meminfo_["zfs_size"];
  } else {
    // Old kernel; give a best-effort approximation of available memory.
    memfree = meminfo_["MemFree"] + meminfo_["Buffers"] + meminfo_["Cached"] +
              meminfo_["SReclaimable"] - meminfo_["Shmem"] + meminfo_["zfs_size"];
  }

  if (memtotal > 0 && memfree >= 0) {
    int used_ram_percentage = 100 * (memtotal - memfree) / memtotal;
    int used_swap_percentage = 0;
    if ((bool) swaptotal) {
      used_swap_percentage = 100 * (swaptotal - swapfree) / swaptotal;
    }

    float divisor = calc_divisor(unit_);
    float total_ram = memtotal / divisor;
    float total_swap = swaptotal / divisor;
    float used_ram = (memtotal - memfree) / divisor;
    float used_swap = (swaptotal - swapfree) / divisor;
    float available_ram = memfree / divisor;
    float available_swap = swapfree / divisor;

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
          fmt::arg("total", total_ram), fmt::arg("swapTotal", total_swap),
          fmt::arg("percentage", used_ram_percentage),
          fmt::arg("swapState", swaptotal == 0 ? "Off" : "On"),
          fmt::arg("swapPercentage", used_swap_percentage), fmt::arg("used", used_ram),
          fmt::arg("swapUsed", used_swap), fmt::arg("avail", available_ram),
          fmt::arg("swapAvail", available_swap)));
    }

    if (tooltipEnabled()) {
      if (config_["tooltip-format"].isString()) {
        auto tooltip_format = config_["tooltip-format"].asString();
        label_.set_tooltip_markup(fmt::format(
            fmt::runtime(tooltip_format), used_ram_percentage,
            fmt::arg("total", total_ram), fmt::arg("swapTotal", total_swap),
            fmt::arg("percentage", used_ram_percentage),
            fmt::arg("swapState", swaptotal == 0 ? "Off" : "On"),
            fmt::arg("swapPercentage", used_swap_percentage), fmt::arg("used", used_ram),
            fmt::arg("swapUsed", used_swap), fmt::arg("avail", available_ram),
            fmt::arg("swapAvail", available_swap)));
      } else {
        label_.set_tooltip_markup(fmt::format("{:.{}f}GiB used", used_ram, 1));
      }
    }
  } else {
    event_box_.hide();
  }
  // Call parent update
  ALabel::update();
}

float waybar::modules::Memory::calc_divisor(const std::string& divisor) {
  if (divisor == "kB") {
    return 1.0;
  } else if (divisor == "kiB") {
    return 1.024;
  } else if (divisor == "MB") {
    return 1.000 * 1000.0;
  } else if (divisor == "MiB") {
    return 1.024 * 1024.0;
  } else if (divisor == "GB") {
    return 1.000 * 1000.0 * 1000.0;
  } else if (divisor == "GiB") {
    return 1.024 * 1024.0 * 1024.0;
  } else if (divisor == "TB") {
    return 1.000 * 1000.0 * 1000.0 * 1000.0;
  } else if (divisor == "TiB") {
    return 1.024 * 1024.0 * 1024.0 * 1024.0;
  } else {  // default to GiB if it is anything that we don't recongnise
    return 1.024 * 1024.0 * 1024.0;
  }
}
