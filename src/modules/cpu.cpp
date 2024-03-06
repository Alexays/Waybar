#include "modules/cpu.hpp"

#include "modules/cpu_frequency.hpp"
#include "modules/cpu_usage.hpp"
#include "modules/load.hpp"

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
  auto [load1, load5, load15] = Load::getLoad();
  auto [cpu_usage, tooltip] = CpuUsage::getCpuUsage(prev_times_);
  auto [max_frequency, min_frequency, avg_frequency] = CpuFrequency::getCpuFrequency();
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
    store.push_back(fmt::arg("load", load1));
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
