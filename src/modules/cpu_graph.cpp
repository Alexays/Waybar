#include "modules/cpu_graph.hpp"

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

waybar::modules::CpuGraph::CpuGraph(const std::string& id, const Json::Value& config)
    : AGraph(config, "cpu_graph", id, 5) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::CpuGraph::update() -> void {
  // TODO: as creating dynamic fmt::arg arrays is buggy we have to calc both
  auto [cpu_usage, tooltip] = CpuUsage::getCpuUsage(prev_times_);
  if (tooltipEnabled()) {
    graph_.set_tooltip_text(tooltip);
  }
  auto total_usage = cpu_usage.empty() ? 0 : cpu_usage[0];
  addValue(total_usage);

  graph_.get_style_context()->remove_class(MODERATE_CLASS);
  graph_.get_style_context()->remove_class(HIGH_CLASS);
  graph_.get_style_context()->remove_class(INTENSIVE_CLASS);

  if (total_usage > 90) {
    graph_.get_style_context()->add_class(INTENSIVE_CLASS);
  } else if (total_usage > 70) {
    graph_.get_style_context()->add_class(HIGH_CLASS);
  } else if (total_usage > 30) {
    graph_.get_style_context()->add_class(MODERATE_CLASS);
  }

  // Call parent update
  AGraph::update();
}
