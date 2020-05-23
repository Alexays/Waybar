#include "modules/memory.hpp"

namespace waybar::modules {

Memory::Memory(const std::string& id, const Json::Value& config)
    : ALabel(config, "memory", id, "{}%", "{used:.1f}Gb used", 30) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto Memory::update(std::string format, fmt::dynamic_format_arg_store<fmt::format_context>& args)
    -> void {
  // Get memory infos
  auto meminfo = parseMeminfo();

  // Hide if wen can't have MemTotal info
  if (meminfo.find("MemTotal") == meminfo.end()) {
    event_box_.hide();
    return;
  }

  unsigned long memfree = 0;
  auto memtotal = meminfo["MemTotal"];
  if (meminfo.count("MemAvailable")) {
    // New kernels (3.4+) have an accurate available memory field.
    memfree = meminfo["MemAvailable"];
  } else {
    // Old kernel; give a best-effort approximation of available memory.
    memfree = meminfo["MemFree"] + meminfo["Buffers"] + meminfo["Cached"] +
              meminfo["SReclaimable"] - meminfo["Shmem"];
  }

  // Add default percentage arg
  int used_ram_percentage = 100 * (memtotal - memfree) / memtotal;
  args.push_back(used_ram_percentage);
  auto percentageArg = fmt::arg("percentage", used_ram_percentage);
  args.push_back(std::cref(percentageArg));
  getState(used_ram_percentage);

  if (ALabel::hasFormat("total")) {
    auto total_ram_gigabytes = memtotal / std::pow(1024, 2);
    auto totalArg = fmt::arg("total", total_ram_gigabytes);
    args.push_back(std::cref(totalArg));
  }

  // Used arg is used for default tooltip
  if (ALabel::hasFormat("used") || AModule::tooltipEnabled()) {
    auto used_ram_gigabytes = (memtotal - memfree) / std::pow(1024, 2);
    auto usedArg = fmt::arg("used", used_ram_gigabytes);
    args.push_back(std::cref(usedArg));
  }

  if (ALabel::hasFormat("avail")) {
    auto available_ram_gigabytes = memfree / std::pow(1024, 2);
    auto availArg = fmt::arg("avail", available_ram_gigabytes);
    args.push_back(std::cref(availArg));
  }

  // Call parent update
  ALabel::update(format, args);
}

}  // namespace waybar::modules
