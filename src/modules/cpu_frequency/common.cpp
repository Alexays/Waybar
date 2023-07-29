#include "modules/cpu_frequency.hpp"

// In the 80000 version of fmt library authors decided to optimize imports
// and moved declarations required for fmt::dynamic_format_arg_store in new
// header fmt/args.h
#if (FMT_VERSION >= 80000)
#include <fmt/args.h>
#else
#include <fmt/core.h>
#endif

waybar::modules::CpuFrequency::CpuFrequency(const std::string& id, const Json::Value& config)
    : ALabel(config, "cpu_frequency", id, "{avg_frequency}", 10) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::CpuFrequency::update() -> void {
  // TODO: as creating dynamic fmt::arg arrays is buggy we have to calc both
  auto [max_frequency, min_frequency, avg_frequency] = CpuFrequency::getCpuFrequency();
  if (tooltipEnabled()) {
    auto tooltip =
        fmt::format("Minimum frequency: {}\nAverage frequency: {}\nMaximum frequency: {}\n",
                    min_frequency, avg_frequency, max_frequency);
    label_.set_tooltip_text(tooltip);
  }
  auto format = format_;
  auto state = getState(avg_frequency);
  if (!state.empty() && config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  }

  if (format.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
    auto icons = std::vector<std::string>{state};
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    store.push_back(fmt::arg("icon", getIcon(avg_frequency, icons)));
    store.push_back(fmt::arg("max_frequency", max_frequency));
    store.push_back(fmt::arg("min_frequency", min_frequency));
    store.push_back(fmt::arg("avg_frequency", avg_frequency));
    label_.set_markup(fmt::vformat(format, store));
  }

  // Call parent update
  ALabel::update();
}

std::tuple<float, float, float> waybar::modules::CpuFrequency::getCpuFrequency() {
  std::vector<float> frequencies = CpuFrequency::parseCpuFrequencies();
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
