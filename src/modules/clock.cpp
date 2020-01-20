#include "modules/clock.hpp"
#include <date/tz.h>

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60) {
  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now + interval_);
    auto diff = std::chrono::seconds(timeout.time_since_epoch().count() % interval_.count());
    thread_.sleep_until(timeout - diff);
  };
}

auto waybar::modules::Clock::update() -> void {
  tzset(); // Update timezone information
  const date::time_zone* zone;
  auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  if (config_["timezone"].isString()) {
    zone = date::locate_zone(config_["timezone"].asString());
  } else {
    zone = date::current_zone();
  }
  auto localtime = date::make_zoned(zone, now);
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

template <typename ZonedTimeInner>
struct fmt::formatter<date::zoned_time<ZonedTimeInner>> {

  std::string *format_string;

  constexpr auto parse(format_parse_context& ctx) {
    format_string = new std::string[1];
    auto it = ctx.begin(), end = ctx.end();
    while (it != (end - 1)) {
      *format_string += *it++;
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const date::zoned_time<ZonedTimeInner>& d, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", date::format(*format_string, d));
  }
};
