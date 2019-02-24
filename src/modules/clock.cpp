#include "modules/clock.hpp"

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
  : ALabel(config, "{:%H:%M}", 60)
{
  label_.set_name("clock");
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  thread_ = [this] {
    auto now = std::chrono::system_clock::now();
    dp.emit();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now + interval_);
    auto time_s = std::chrono::time_point_cast<std::chrono::seconds>(timeout);
    auto sub_m =
      std::chrono::duration_cast<std::chrono::seconds>(time_s.time_since_epoch()).count() % interval_.count();
    thread_.sleep_until(timeout - std::chrono::seconds(sub_m - 1));
  };
}

auto waybar::modules::Clock::update() -> void
{
  auto localtime = fmt::localtime(std::time(nullptr));
  auto text = fmt::format(format_, localtime);
  label_.set_markup(text);
  
  if (tooltipEnabled()) {
    if (config_["tooltip-format"].isString()) {
      auto tooltip_format = config_["tooltip-format"].asString();
      auto tooltip_text = fmt::format(tooltip_format, localtime);
      label_.set_tooltip_text(tooltip_text);
    } else {
      label_.set_tooltip_text(text);
    }
  }
}
