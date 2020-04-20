#include "modules/cpu.hpp"

#include <numeric>

namespace waybar::modules {

Cpu::Cpu(const std::string& id, const Json::Value& config)
    : ALabel(config, "cpu", id, "{usage}%", "", 10) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto Cpu::update(std::string format, ALabel::args& args) -> void {
  if (ALabel::hasFormat("load")) {
    auto cpu_load = getCpuLoad();
    auto loadArg = fmt::arg("load", cpu_load);
    args.push_back(std::cref(loadArg));
  }

  // Usage is the default format and also the state one
  auto [cpu_usage, tooltip] = getCpuUsage();
  auto usageArg = fmt::arg("usage", cpu_usage);
  args.push_back(std::cref(usageArg));
  if (AModule::tooltipEnabled()) {
    label_.set_tooltip_text(tooltip);
  }
  getState(cpu_usage);

  // Call parent update
  ALabel::update(format, args);
}

uint16_t Cpu::getCpuLoad() {
  struct sysinfo info = {0};
  if (sysinfo(&info) == 0) {
    float f_load = 1.F / (1U << SI_LOAD_SHIFT);
    uint16_t load = info.loads[0] * f_load * 100 / get_nprocs();
    return load;
  }
  throw std::runtime_error("Can't get Cpu load");
}

std::tuple<uint16_t, std::string> Cpu::getCpuUsage() {
  if (prev_times_.empty()) {
    prev_times_ = parseCpuinfo();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::vector<std::tuple<size_t, size_t>> curr_times = parseCpuinfo();
  std::string tooltip;
  uint16_t usage = 0;
  for (size_t i = 0; i < curr_times.size(); ++i) {
    auto [curr_idle, curr_total] = curr_times[i];
    auto [prev_idle, prev_total] = prev_times_[i];
    const float delta_idle = curr_idle - prev_idle;
    const float delta_total = curr_total - prev_total;
    uint16_t tmp = 100 * (1 - delta_idle / delta_total);
    if (i == 0) {
      usage = tmp;
      tooltip = fmt::format("Total: {}%", tmp);
    } else {
      tooltip = tooltip + fmt::format("\nCore{}: {}%", i - 1, tmp);
    }
  }
  prev_times_ = curr_times;
  return {usage, tooltip};
}

std::vector<std::tuple<size_t, size_t>> Cpu::parseCpuinfo() {
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

}  // namespace waybar::modules
