#include "modules/cpu.hpp"

waybar::modules::Cpu::Cpu(const std::string& id, const Json::Value& config)
    : ALabel(config, "cpu", id, "{usage}%", 10) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::Cpu::update() -> void {
  // TODO: as creating dynamic fmt::arg arrays is buggy we have to calc both
  auto cpu_load = getCpuLoad();
  auto [cpu_usage, tooltip] = getCpuUsage();
  auto [max_frequency, min_frequency, avg_frequency] = getCpuFrequency();
  if (tooltipEnabled()) {
    label_.set_tooltip_text(tooltip);
  }
  auto format = format_;
  auto state = getState(cpu_usage);
  if (!state.empty() && config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  }

  if (format.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
    label_.set_markup(fmt::format(format,
                                  fmt::arg("load", cpu_load),
                                  fmt::arg("usage", cpu_usage),
                                  fmt::arg("max_frequency", max_frequency),
                                  fmt::arg("min_frequency", min_frequency),
                                  fmt::arg("avg_frequency", avg_frequency)));
  }

  // Call parent update
  ALabel::update();
}

double waybar::modules::Cpu::getCpuLoad() {
  double load[1];
  if (getloadavg(load, 1) != -1) {
    return load[0];
  }
  throw std::runtime_error("Can't get Cpu load");
}

std::tuple<uint16_t, std::string> waybar::modules::Cpu::getCpuUsage() {
  if (prev_times_.empty()) {
    prev_times_ = parseCpuinfo();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::vector<std::tuple<size_t, size_t>> curr_times = parseCpuinfo();
  std::string                             tooltip;
  uint16_t                                usage = 0;
  for (size_t i = 0; i < curr_times.size(); ++i) {
    auto [curr_idle, curr_total] = curr_times[i];
    auto [prev_idle, prev_total] = prev_times_[i];
    const float delta_idle = curr_idle - prev_idle;
    const float delta_total = curr_total - prev_total;
    uint16_t    tmp = 100 * (1 - delta_idle / delta_total);
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

std::tuple<float, float, float> waybar::modules::Cpu::getCpuFrequency() {
  std::vector<float> frequencies = parseCpuFrequencies();
  auto [min, max] = std::minmax_element(std::begin(frequencies), std::end(frequencies));
  float avg_frequency = std::accumulate(std::begin(frequencies), std::end(frequencies), 0.0) / frequencies.size();

  // Round frequencies with double decimal precision to get GHz
  float max_frequency = std::ceil(*max / 10.0) / 100.0;
  float min_frequency = std::ceil(*min / 10.0) / 100.0;
  avg_frequency = std::ceil(avg_frequency / 10.0) / 100.0;

  return { max_frequency, min_frequency, avg_frequency };
}
