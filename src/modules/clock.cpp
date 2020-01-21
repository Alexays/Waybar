#include "modules/clock.hpp"

waybar::modules::Clock::Clock(const std::string& id, const Json::Value& config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60)
    , fixed_time_zone_(false)
{
  if (config_["timezone"].isString()) {
    time_zone_ = date::locate_zone(config_["timezone"].asString());
    fixed_time_zone_ = true;
  }

  if (config_["locale"].isString()) {
    locale_ = std::locale(config_["locale"].asString());
  } else {
    locale_ = std::locale("");
  }

  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now + interval_);
    auto diff = std::chrono::seconds(timeout.time_since_epoch().count() % interval_.count());
    thread_.sleep_until(timeout - diff);
  };
}

using zoned_time = date::zoned_time<std::chrono::system_clock::duration>;

struct waybar_time {
  std::locale locale;
  zoned_time ztime;
};

auto waybar::modules::Clock::update() -> void {
  if (!fixed_time_zone_) {
    // Time zone can change. Be sure to pick that.
    time_zone_ = date::current_zone();
  }
  auto now = std::chrono::system_clock::now();
  waybar_time wtime = {locale_, date::make_zoned(time_zone_, now)};

  auto text = fmt::format(format_, wtime);
  label_.set_markup(text);

  if (tooltipEnabled()) {
    if (config_["tooltip-format"].isString()) {
      auto tooltip_format = config_["tooltip-format"].asString();
      auto tooltip_text = fmt::format(tooltip_format, wtime);
      label_.set_tooltip_text(tooltip_text);
    } else {
      label_.set_tooltip_text(text);
    }
  }
}

template <>
struct fmt::formatter<waybar_time> : fmt::formatter<std::tm> {
  template <typename FormatContext>
  auto format(const waybar_time& t, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", date::format(t.locale, fmt::to_string(tm_format), t.ztime));
  }
};
