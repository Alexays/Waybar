#include "modules/memory.hpp"

namespace waybar::modules {

Memory::Memory(const std::string& id, const Json::Value& config)
    : ALabel(config, "memory", id, "{}%", "{used:.1f}Gb used", 30) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto Memory::update(std::string format, waybar::args &args) -> void {
  // Get memory infos
  auto meminfo = parseMeminfo();

  // Hide if wen can't have MemTotal info
  if (meminfo.find("MemTotal") == m.end()) {
    event_box_.hide();
    return;
  }

  unsigned long memfree = 0;
  auto memtotal = meminfo_["MemTotal"];
  if (meminfo_.count("MemAvailable")) {
    // New kernels (3.4+) have an accurate available memory field.
    memfree = meminfo_["MemAvailable"];
  } else {
    // Old kernel; give a best-effort approximation of available memory.
    memfree = meminfo_["MemFree"] + meminfo_["Buffers"] + meminfo_["Cached"] +
              meminfo_["SReclaimable"] - meminfo_["Shmem"];
  }

  // Add default percentage arg
  int used_ram_percentage = 100 * (memtotal - memfree) / memtotal;
  args.push_back(used_ram_percentage);
  args.push_back(fmt::arg("percentage", used_ram_percentage));
  getState(used_ram_percentage);

  if (ALabel::hasFormat("total")) {
    auto total_ram_gigabytes = memtotal / std::pow(1024, 2);
    args.push_back(fmt::arg("total", total_ram_gigabytes));
  }

  // Used arg is used for default tooltip
  if (ALabel::hasFormat("used") || AModule::tooltipEnabled()) {
    auto used_ram_gigabytes = (memtotal - memfree) / std::pow(1024, 2);
    args.push_back(fmt::arg("used", used_ram_gigabytes));
  }

  if (ALabel::hasFormat("avail")) {
    auto available_ram_gigabytes = memfree / std::pow(1024, 2);
    args.push_back(fmt::arg("avail", available_ram_gigabytes));
  }

  // Call parent update
  ALabel::update(format, args);
}

std::unordered_map<std::string, unsigned long> Memory::parseMeminfo() {
  std::unordered_map<std::string, unsigned long> meminfo;

  std::ifstream info(data_dir_);
  if (!info.is_open()) {
    throw std::runtime_error("Can't open " + data_dir_);
  }
  std::string line;
  while (getline(info, line)) {
    auto posDelim = line.find(':');
    if (posDelim == std::string::npos) {
      continue;
    }

    std::string name = line.substr(0, posDelim);
    int64_t value = std::stol(line.substr(posDelim + 1));
    meminfo[name] = value;
  }

  return meminfo;
}

}  // namespace waybar::modules
