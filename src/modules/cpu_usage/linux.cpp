#include <filesystem>

#include "modules/cpu_usage.hpp"

std::vector<std::tuple<size_t, size_t>> waybar::modules::CpuUsage::parseCpuinfo() {
  const std::string data_dir_ = "/proc/stat";
  std::ifstream info(data_dir_);
  if (!info.is_open()) {
    throw std::runtime_error("Can't open " + data_dir_);
  }
  std::vector<std::tuple<size_t, size_t>> cpuinfo;
  std::string line;
  while (getline(info, line)) {
    if (line.substr(0, 3).compare("cpu") != 0) {
      break;
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
  }
  return cpuinfo;
}
