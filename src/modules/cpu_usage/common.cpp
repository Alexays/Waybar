#include "modules/cpu_usage.hpp"

// In the 80000 version of fmt library authors decided to optimize imports
// and moved declarations required for fmt::dynamic_format_arg_store in new
// header fmt/args.h
#if (FMT_VERSION >= 80000)
#include <fmt/args.h>
#else
#include <fmt/core.h>
#endif

waybar::modules::CpuUsage::CpuUsage(const std::string& id, const Json::Value& config)
    : ALabel(config, "cpu_usage", id, "{usage}%", 10) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::CpuUsage::update() -> void {
  // TODO: as creating dynamic fmt::arg arrays is buggy we have to calc both
  auto [cpu_usage, tooltip] = CpuUsage::getCpuUsage(prev_times_);
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
    store.push_back(fmt::arg("usage", total_usage));
    store.push_back(fmt::arg("icon", getIcon(total_usage, icons)));
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

std::tuple<std::vector<uint16_t>, std::string> waybar::modules::CpuUsage::getCpuUsage(
    std::vector<std::tuple<size_t, size_t>>& prev_times) {
  if (prev_times.empty()) {
    prev_times = CpuUsage::parseCpuinfo();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::vector<std::tuple<size_t, size_t>> curr_times = CpuUsage::parseCpuinfo();
  std::string tooltip;
  std::vector<uint16_t> usage;

  if (curr_times.size() != prev_times.size()) {
    // The number of CPUs has changed, eg. due to CPU hotplug
    // We don't know which CPU came up or went down
    // so only give total usage (if we can)
    if (!curr_times.empty() && !prev_times.empty()) {
      auto [curr_idle, curr_total] = curr_times[0];
      auto [prev_idle, prev_total] = prev_times[0];
      const float delta_idle = curr_idle - prev_idle;
      const float delta_total = curr_total - prev_total;
      uint16_t tmp = 100 * (1 - delta_idle / delta_total);
      tooltip = fmt::format("Total: {}%\nCores: (pending)", tmp);
      usage.push_back(tmp);
    } else {
      tooltip = "(pending)";
      usage.push_back(0);
    }
    prev_times = curr_times;
    return {usage, tooltip};
  }

  for (size_t i = 0; i < curr_times.size(); ++i) {
    auto [curr_idle, curr_total] = curr_times[i];
    auto [prev_idle, prev_total] = prev_times[i];
    if (i > 0 && (curr_total == 0 || prev_total == 0)) {
      // This CPU is offline
      tooltip = tooltip + fmt::format("\nCore{}: offline", i - 1);
      usage.push_back(0);
      continue;
    }
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
  prev_times = curr_times;
  return {usage, tooltip};
}
