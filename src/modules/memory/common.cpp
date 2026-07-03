#include "modules/memory.hpp"

namespace {
const std::unordered_map<std::string, float> kUnits = {{"kB", 1.000},
                                                       {"kiB", 1.024},
                                                       {"MB", 1.000 * 1000.0},
                                                       {"MiB", 1.024 * 1024.0},
                                                       {"GB", 1.000 * 1000.0 * 1000.0},
                                                       {"GiB", 1.024 * 1024.0 * 1024.0},
                                                       {"TB", 1.000 * 1000.0 * 1000.0 * 1000.0},
                                                       {"TiB", 1.024 * 1024.0 * 1024.0 * 1024.0}};
}

waybar::modules::Memory::Memory(const std::string& id, const Json::Value& config)
    : ALabel(config, "memory", id, "{}%", 30) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
  if (config["unit"].isString()) {
    unit_ = config["unit"].asString();
    if (!kUnits.contains(unit_)) {
      unit_ = "GiB";
    }
  } else {
    unit_ = "GiB";
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
    if ((bool)swaptotal) {
      used_swap_percentage = 100 * (swaptotal - swapfree) / swaptotal;
    }

    float divisor = kUnits.at(unit_);
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
      updateLabelAndTooltip(
          format, fmt::format("{:.{}f}{} used", used_ram, 1, unit_), used_ram_percentage,
          fmt::arg("icon", getIcon(used_ram_percentage, icons)), fmt::arg("total", total_ram),
          fmt::arg("swapTotal", total_swap), fmt::arg("percentage", used_ram_percentage),
          fmt::arg("swapState", swaptotal == 0 ? "Off" : "On"),
          fmt::arg("swapPercentage", used_swap_percentage), fmt::arg("used", used_ram),
          fmt::arg("swapUsed", used_swap), fmt::arg("avail", available_ram),
          fmt::arg("swapAvail", available_swap));
    }
  } else {
    event_box_.hide();
  }
  // Call parent update
  ALabel::update();
}
