#include <filesystem>

#include "modules/cpu_usage.hpp"

std::vector<std::tuple<size_t, size_t>> waybar::modules::CpuUsage::parseCpuinfo() {
  // Get the "existing CPU count" from /sys/devices/system/cpu/present
  // Probably this is what the user wants the offline CPUs accounted from
  // For further details see:
  // https://www.kernel.org/doc/html/latest/core-api/cpu_hotplug.html
  const std::string sys_cpu_present_path = "/sys/devices/system/cpu/present";
  size_t cpu_present_last = 0;
  std::ifstream cpu_present_file(sys_cpu_present_path);
  std::string cpu_present_text;
  if (cpu_present_file.is_open()) {
    getline(cpu_present_file, cpu_present_text);
    // This is a comma-separated list of ranges, eg. 0,2-4,7
    size_t last_separator = cpu_present_text.find_last_of("-,");
    if (last_separator < cpu_present_text.size()) {
      std::stringstream(cpu_present_text.substr(last_separator + 1)) >> cpu_present_last;
    }
  }

  const std::string data_dir_ = "/proc/stat";
  std::ifstream info(data_dir_);
  if (!info.is_open()) {
    throw std::runtime_error("Can't open " + data_dir_);
  }
  std::vector<std::tuple<size_t, size_t>> cpuinfo;
  std::string line;
  size_t current_cpu_number = -1;  // First line is total, second line is cpu 0
  while (getline(info, line)) {
    if (line.substr(0, 3).compare("cpu") != 0) {
      break;
    }
    size_t line_cpu_number;
    if (current_cpu_number >= 0) {
      std::stringstream(line.substr(3)) >> line_cpu_number;
      while (line_cpu_number > current_cpu_number) {
        // Fill in 0 for offline CPUs missing inside the lines of /proc/stat
        cpuinfo.emplace_back(0, 0);
        current_cpu_number++;
      }
    }
    std::stringstream sline(line.substr(5));
    std::vector<size_t> times;
    for (size_t time = 0; sline >> time; times.push_back(time));

    size_t idle_time = 0;
    size_t total_time = 0;
    if (times.size() >= 5) {
      // idle + iowait
      idle_time = times[3] + times[4];
      total_time = std::accumulate(times.begin(), times.end(), 0);
    }
    cpuinfo.emplace_back(idle_time, total_time);
    current_cpu_number++;
  }

  while (cpu_present_last >= current_cpu_number) {
    // Fill in 0 for offline CPUs missing after the lines of /proc/stat
    cpuinfo.emplace_back(0, 0);
    current_cpu_number++;
  }

  return cpuinfo;
}
