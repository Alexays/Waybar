#include <fmt/args.h>
#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <iterator>
#include <thread>
#include <utility>

#include "modules/cpu_usage.hpp"
#include "util/lazy_format.hpp"

waybar::modules::CpuUsage::CpuUsage(const std::string& id, const Json::Value& config)
    : ALabel(config, "cpu_usage", id, "{usage}%", 10) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::CpuUsage::update() -> void {
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
    FormatArgs usage_args(*this, sample, state);
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    const auto num_args = 3 + sample.usage_.size() * 2;
    store.reserve(num_args, num_args);
    usage_args.addTotal(store);
    usage_args.addCores(store);
    updateLabelAndTooltipForState("", format, store, [&sample] { return sample.defaultTooltip(); });
  }

  // Call parent update
  ALabel::update();
}

void waybar::modules::CpuUsage::FormatArgs::addTotal(
    fmt::dynamic_format_arg_store<fmt::format_context>& store) {
  const auto usage = sample_.totalUsage();
  store.push_back(fmt::arg("usage", usage));
  store.push_back(fmt::arg(
      "icon", util::LazyFormatArg{[this, usage] { return fmt::string_view(icon(usage)); }}));
}

void waybar::modules::CpuUsage::FormatArgs::addCores(
    fmt::dynamic_format_arg_store<fmt::format_context>& store) {
  for (size_t i = 1; i < sample_.usage_.size(); ++i) {
    const auto core = i - 1;
    const auto usage = sample_.usage_[i].value_or(0);
    store.push_back(fmt::arg(fmt::format("usage{}", core).c_str(), usage));
    store.push_back(
        fmt::arg(fmt::format("icon{}", core).c_str(),
                 util::LazyFormatArg{[this, usage] { return fmt::string_view(icon(usage)); }}));
  }
  store.push_back(
      fmt::arg("icons", util::LazyFormatArg{[this] { return fmt::string_view(coreIcons()); }}));
}

const std::string& waybar::modules::CpuUsage::FormatArgs::icon(uint16_t usage) {
  if (const auto found = icons_.find(usage); found != icons_.end()) {
    return found->second;
  }
  return icons_.emplace(usage, label_.getIcon(usage, state_)).first->second;
}

const std::string& waybar::modules::CpuUsage::FormatArgs::coreIcons() {
  if (!core_icons_) {
    std::string icons;
    for (size_t i = 1; i < sample_.usage_.size(); ++i) {
      icons += icon(sample_.usage_[i].value_or(0));
    }
    core_icons_ = std::move(icons);
  }
  return *core_icons_;
}

waybar::modules::CpuUsage::Sample waybar::modules::CpuUsage::getCpuUsage(
    std::vector<std::tuple<size_t, size_t>>& prev_times) {
  if (prev_times.empty()) {
    prev_times = CpuUsage::parseCpuinfo();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  auto curr_times = CpuUsage::parseCpuinfo();
  Sample sample;
  sample.cores_pending_ = curr_times.size() != prev_times.size();
  // After a topology change, only the total has a usable baseline.
  const auto count = sample.cores_pending_
                         ? std::min({size_t{1}, curr_times.size(), prev_times.size()})
                         : curr_times.size();
  sample.usage_.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    auto [curr_idle, curr_total] = curr_times[i];
    auto [prev_idle, prev_total] = prev_times[i];
    if (i > 0 && (curr_total == 0 || prev_total == 0)) {
      sample.usage_.emplace_back(std::nullopt);
      continue;
    }
    const float delta_idle = curr_idle - prev_idle;
    const float delta_total = curr_total - prev_total;
    const uint16_t usage =
        (delta_total > 0) ? static_cast<uint16_t>(100 * (1 - delta_idle / delta_total)) : 0;
    sample.usage_.emplace_back(usage);
  }
  prev_times = std::move(curr_times);
  return sample;
}

std::string waybar::modules::CpuUsage::Sample::defaultTooltip() const {
  if (usage_.empty()) {
    return cores_pending_ ? "(pending)" : "";
  }
  if (cores_pending_) {
    return fmt::format("Total: {}%\nCores: (pending)", totalUsage());
  }

  std::string tooltip;
  auto out = std::back_inserter(tooltip);
  fmt::format_to(out, "Total: {}%", totalUsage());
  for (size_t i = 1; i < usage_.size(); ++i) {
    if (usage_[i]) {
      fmt::format_to(out, "\nCore{}: {}%", i - 1, *usage_[i]);
    } else {
      fmt::format_to(out, "\nCore{}: offline", i - 1);
    }
  }
  return tooltip;
}
