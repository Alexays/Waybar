#include "modules/cpu.hpp"

waybar::modules::Cpu::Cpu(const Json::Value& config)
  : ALabel(config, "{}%")
{
  label_.set_name("cpu");
  uint32_t interval = config_["interval"].isUInt() ? config_["interval"].asUInt() : 10;
  thread_ = [this, interval] {
    dp.emit();
    thread_.sleep_for(chrono::seconds(interval));
  };
}

auto waybar::modules::Cpu::update() -> void
{
  if (prevTimes_.size() < 1) {
    prevTimes_ = parseCpuinfo();
    std::this_thread::sleep_for(chrono::milliseconds(100));
  }
  std::vector< std::tuple<size_t, size_t> > currTimes = parseCpuinfo();
  std::string tooltip;
  for (size_t i = 0; i < currTimes.size(); ++i) {
    auto [currIdle, currTotal] = currTimes[i];
    auto [prevIdle, prevTotal] = prevTimes_[i];
    const float deltaIdle = currIdle - prevIdle;
    const float deltaTotal = currTotal - prevTotal;
    uint16_t load = 100 * (1 - deltaIdle / deltaTotal);
    if (i == 0) {
      label_.set_text(fmt::format(format_, load));
      tooltip = fmt::format("Total: {}%", load);
    } else {
      tooltip = tooltip + fmt::format("\nCore{}: {}%", i - 1, load);
    } 
  }
  label_.set_tooltip_text(tooltip);
  prevTimes_ = currTimes;
}

std::vector< std::tuple<size_t, size_t> > waybar::modules::Cpu::parseCpuinfo()
{
  std::ifstream info(data_dir_);
  if (!info.is_open()) {
    throw std::runtime_error("Can't open " + data_dir_);
  }
  std::vector< std::tuple<size_t, size_t> > cpuinfo;
  std::string line;
  while (getline(info, line)) {    
    if (line.substr(0,3).compare("cpu") != 0) {
      break;
    }
    std::stringstream sline(line.substr(5));
    std::vector<size_t> times;
    for (size_t time; sline >> time; times.push_back(time));

    size_t idle_time = 0;
    size_t total_time = 0;
    if (times.size() >= 4) {
      idle_time = times[3];
      total_time = std::accumulate(times.begin(), times.end(), 0);
    }
    cpuinfo.push_back( {idle_time, total_time} );
  }
  return cpuinfo;
}
