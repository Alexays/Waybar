#include "modules/cpu.hpp"

// In the 80000 version of fmt library authors decided to optimize imports
// and moved declarations required for fmt::dynamic_format_arg_store in new
// header fmt/args.h
#if (FMT_VERSION >= 80000)
#include <fmt/args.h>
#else
#include <fmt/core.h>
#endif

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
  auto total_usage = cpu_usage.empty() ? 0 : cpu_usage[0];
  auto state = getState(total_usage);
  if (!state.empty() && config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  }

  if (format.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
    auto icons = std::vector<std::string>{state};
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    store.push_back(fmt::arg("load", cpu_load));
    store.push_back(fmt::arg("load", cpu_load));
    store.push_back(fmt::arg("usage", total_usage));
    store.push_back(fmt::arg("icon", getIcon(total_usage, icons)));
    store.push_back(fmt::arg("max_frequency", max_frequency));
    store.push_back(fmt::arg("min_frequency", min_frequency));
    store.push_back(fmt::arg("avg_frequency", avg_frequency));
    for (size_t i = 1; i < cpu_usage.size(); ++i) {
      auto core_i = i - 1;
      auto core_format = fmt::format("usage{}", core_i);
      store.push_back(fmt::arg(core_format.c_str(), cpu_usage[i]));
      auto icon_format = fmt::format("icon{}", core_i);
      store.push_back(fmt::arg(icon_format.c_str(), getIcon(cpu_usage[i], icons)));
    }
    label_.set_markup(fmt::vformat(format, store));
  }

  // Call parent update
  ALabel::update();
}

double waybar::modules::Cpu::getCpuLoad() {
  double load[1];
  if (getloadavg(load, 1) != -1) {
    return std::ceil(load[0] * 100.0) / 100.0;
  }
  throw std::runtime_error("Can't get Cpu load");
}

std::tuple<std::vector<uint16_t>, std::string> waybar::modules::Cpu::getCpuUsage() {
  if (prev_times_.empty()) {
    prev_times_ = parseCpuinfo();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::vector<std::tuple<size_t, size_t>> curr_times = parseCpuinfo();
  std::string tooltip;
  std::vector<uint16_t> usage;
  for (size_t i = 0; i < curr_times.size(); ++i) {
    auto [curr_idle, curr_total] = curr_times[i];
    auto [prev_idle, prev_total] = prev_times_[i];
    const float delta_idle = curr_idle - prev_idle;
    const float delta_total = curr_total - prev_total;
    uint16_t tmp = 100 * (1 - delta_idle / delta_total);
    if (i == 0) {
      tooltip = fmt::format("Total: {}%", tmp);
    } else {
      tooltip = tooltip + fmt::format("\nCore{}: {}%", i - 1, tmp);
    }
    usage.push_back(tmp);
  }
  prev_times_ = curr_times;
  return {usage, tooltip};
}

std::tuple<float, float, float> waybar::modules::Cpu::getCpuFrequency() {
  std::vector<float> frequencies = parseCpuFrequencies();
  if (frequencies.empty()) {
    return {0.f, 0.f, 0.f};
  }
  auto [min, max] = std::minmax_element(std::begin(frequencies), std::end(frequencies));
  float avg_frequency =
      std::accumulate(std::begin(frequencies), std::end(frequencies), 0.0) / frequencies.size();

  // Round frequencies with double decimal precision to get GHz
  float max_frequency = std::ceil(*max / 10.0) / 100.0;
  float min_frequency = std::ceil(*min / 10.0) / 100.0;
  avg_frequency = std::ceil(avg_frequency / 10.0) / 100.0;

  return {max_frequency, min_frequency, avg_frequency};
}
