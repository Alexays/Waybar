#include "modules/load.hpp"

// In the 80000 version of fmt library authors decided to optimize imports
// and moved declarations required for fmt::dynamic_format_arg_store in new
// header fmt/args.h
#if (FMT_VERSION >= 80000)
#include <fmt/args.h>
#else
#include <fmt/core.h>
#endif

waybar::modules::Load::Load(const std::string& id, const Json::Value& config)
    : ALabel(config, "load", id, "{load1}", 10) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::Load::update() -> void {
  // TODO: as creating dynamic fmt::arg arrays is buggy we have to calc both
  auto [load1, load5, load15] = Load::getLoad();
  if (tooltipEnabled()) {
    auto tooltip = fmt::format("Load 1: {}\nLoad 5: {}\nLoad 15: {}", load1, load5, load15);
    label_.set_tooltip_text(tooltip);
  }
  auto format = format_;
  auto state = getState(load1);
  if (!state.empty() && config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  }

  if (format.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
    auto icons = std::vector<std::string>{state};
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    store.push_back(fmt::arg("load1", load1));
    store.push_back(fmt::arg("load5", load5));
    store.push_back(fmt::arg("load15", load15));
    store.push_back(fmt::arg("icon1", getIcon(load1, icons)));
    store.push_back(fmt::arg("icon5", getIcon(load5, icons)));
    store.push_back(fmt::arg("icon15", getIcon(load15, icons)));
    label_.set_markup(fmt::vformat(format, store));
  }

  // Call parent update
  ALabel::update();
}

std::tuple<double, double, double> waybar::modules::Load::getLoad() {
  double load[3];
  if (getloadavg(load, 3) != -1) {
    double load1 = std::ceil(load[0] * 100.0) / 100.0;
    double load5 = std::ceil(load[1] * 100.0) / 100.0;
    double load15 = std::ceil(load[2] * 100.0) / 100.0;
    return {load1, load5, load15};
  }
  throw std::runtime_error("Can't get system load");
}
