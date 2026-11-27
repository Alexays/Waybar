#include "modules/cpu.hpp"

#include <fmt/args.h>

#include <concepts>
#include <cstddef>
#include <optional>
#include <tuple>

#include "modules/cpu_frequency.hpp"
#include "modules/cpu_usage.hpp"
#include "modules/load.hpp"
#include "util/lazy_format.hpp"

namespace {
// Read a tuple of CPU values when its first field is rendered and reuse the
// sample for every label or tooltip field during this update.
template <std::invocable auto Read>
class CpuSample {
 public:
  template <std::size_t Index>
  auto arg() {
    return waybar::util::LazyFormatArg{[this] {
      if (!values_) {
        values_ = Read();
      }
      return std::get<Index>(*values_);
    }};
  }

 private:
  std::optional<decltype(Read())> values_;
};
}  // namespace

waybar::modules::Cpu::Cpu(const std::string& id, const Json::Value& config)
    : ALabel(config, "cpu", id, "{usage}%", 10) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::Cpu::update() -> void {
  const auto sample = CpuUsage::getCpuUsage(prev_times_);

  auto format = format_;
  auto total_usage = sample.totalUsage();
  auto state = getState(total_usage);
  if (!state.empty() && config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  }

  if (format.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
    CpuSample<Load::getLoad> load;
    CpuSample<CpuFrequency::getCpuFrequency> frequencies;
    CpuUsage::FormatArgs usage_args(*this, sample, state);
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    const auto num_args = 10 + sample.usage_.size() * 2;
    store.reserve(num_args, num_args);
    store.push_back(fmt::arg("load", load.arg<0>()));
    store.push_back(fmt::arg("load1", load.arg<0>()));
    store.push_back(fmt::arg("load5", load.arg<1>()));
    store.push_back(fmt::arg("load15", load.arg<2>()));
    usage_args.addTotal(store);
    store.push_back(fmt::arg("max_frequency", frequencies.arg<0>()));
    store.push_back(fmt::arg("min_frequency", frequencies.arg<1>()));
    store.push_back(fmt::arg("avg_frequency", frequencies.arg<2>()));
    usage_args.addCores(store);
    updateLabelAndTooltipForState(state, format, store,
                                  [&sample] { return sample.defaultTooltip(); });
  }

  // Call parent update
  ALabel::update();
}
