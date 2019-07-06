#include "modules/cpu.hpp"
#include <numeric>

namespace waybar {

modules::Cpu::Cpu(const std::string& id, const Json::Value& config)
    : ALabel(config, "cpu", id, "{usage}%", 10) {
  args_.emplace("load", std::bind(&Cpu::getCpuLoad, this));
  args_.emplace("usage", std::bind(&Cpu::getCpuUsage, this));
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

uint16_t modules::Cpu::getCpuLoad() {
  struct sysinfo info = {0};
  if (sysinfo(&info) == 0) {
    float    f_load = 1.F / (1U << SI_LOAD_SHIFT);
    uint16_t load = info.loads[0] * f_load * 100 / get_nprocs();
    return load;
  }
  throw std::runtime_error("Can't get Cpu load");
}

uint16_t modules::Cpu::getCpuUsage() {
  if (prev_times_.empty()) {
    prev_times_ = parseCpuinfo();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::vector<std::tuple<size_t, size_t>> curr_times = parseCpuinfo();
  uint16_t                                usage = 0;
  for (size_t i = 0; i < curr_times.size(); ++i) {
    auto [curr_idle, curr_total] = curr_times[i];
    auto [prev_idle, prev_total] = prev_times_[i];
    const float delta_idle = curr_idle - prev_idle;
    const float delta_total = curr_total - prev_total;
    uint16_t    tmp = 100 * (1 - delta_idle / delta_total);
    if (i == 0) {
      usage = tmp;
    }
  }
  prev_times_ = curr_times;
  return usage;
}

std::vector<std::tuple<size_t, size_t>> modules::Cpu::parseCpuinfo() {
  std::ifstream info(data_dir_);
  if (!info.is_open()) {
    throw std::runtime_error("Can't open " + data_dir_);
  }
  std::vector<std::tuple<size_t, size_t>> cpuinfo;
  std::string                             line;
  while (getline(info, line)) {
    if (line.substr(0, 3).compare("cpu") != 0) {
      break;
    }
    std::stringstream   sline(line.substr(5));
    std::vector<size_t> times;
    for (size_t time = 0; sline >> time; times.push_back(time))
      ;

    size_t idle_time = 0;
    size_t total_time = 0;
    if (times.size() >= 4) {
      idle_time = times[3];
      total_time = std::accumulate(times.begin(), times.end(), 0);
    }
    cpuinfo.emplace_back(idle_time, total_time);
  }
  return cpuinfo;
}

}  // namespace waybar
