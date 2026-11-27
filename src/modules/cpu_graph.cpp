#include "modules/cpu_graph.hpp"

#include "modules/cpu_usage.hpp"

waybar::modules::CpuGraph::CpuGraph(const std::string& id, const Json::Value& config)
    : AGraph(config, "cpu_graph", id, 5) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::CpuGraph::update() -> void {
  const auto sample = CpuUsage::getCpuUsage(prev_times_);
  if (tooltipEnabled()) {
    graph_.set_tooltip_text(sample.defaultTooltip());
  }
  auto total_usage = sample.totalUsage();
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
