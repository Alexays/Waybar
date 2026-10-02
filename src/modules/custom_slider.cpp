#include "modules/custom_slider.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <optional>

#include "util/command.hpp"

namespace waybar::modules {

CustomSlider::CustomSlider(const std::string& name, const std::string& id,
                           const Json::Value& config, const std::string& output_name)
    : ASlider(config, "custom-slider-" + name, id),
      name_(name),
      on_change_(config["on-change"].isString() ? config["on-change"].asString() : ""),
      source_(config, output_name, [this] { this->dp.emit(); }) {}

auto CustomSlider::update() -> void {
  auto out = source_.parse();
  if (!out.ok) {
    setStale(true);  // failed or empty read: keep the last value, never 0
    return;
  }
  // Prefer an explicit json percentage; otherwise read a number from the text.
  std::optional<double> value =
      out.percentage.has_value() ? std::optional<double>(*out.percentage) : parseValue(out.text);
  if (!value) {
    setStale(true);  // unparseable output: stale, not 0
    return;
  }
  setStale(false);

  if (tooltipEnabled()) {
    scale_.set_tooltip_markup(out.tooltip);
  }
  auto style = scale_.get_style_context();
  for (const auto& c : prev_classes_) {
    style->remove_class(c);
  }
  for (const auto& c : out.classes) {
    style->add_class(c);
  }
  prev_classes_ = out.classes;

  // A poll must not snap the handle out from under an active drag.
  if (commitPending()) {
    return;
  }
  setValueSilently(std::clamp(static_cast<int>(std::lround(*value)), min_, max_));
}

void CustomSlider::onCommit(int value) {
  if (on_change_.empty()) {
    return;
  }
  // {} is the only interpolation; the command itself is user-supplied (custom/ trust model).
  try {
    util::command::execNoRead(fmt::format(fmt::runtime(on_change_), value));
  } catch (const fmt::format_error& e) {
    spdlog::error("custom-slider {}: on-change format error: {}", name_, e.what());
  }
}

auto CustomSlider::refresh(int signal) -> void { source_.refresh(signal); }

}  // namespace waybar::modules
