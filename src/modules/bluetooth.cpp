#include "modules/bluetooth.hpp"
#include "util/rfkill.hpp"
#include <linux/rfkill.h>
#include <time.h>

waybar::modules::Bluetooth::Bluetooth(const std::string& id, const Json::Value& config)
    : ALabel(config, "bluetooth", id, "{icon}", 10),
      status_("disabled"),
      rfkill_{RFKILL_TYPE_BLUETOOTH} {
  thread_ = [this] {
    dp.emit();
    rfkill_.waitForEvent();
  };
  intervall_thread_ = [this] {
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now + interval_);
    auto diff = std::chrono::seconds(timeout.time_since_epoch().count() % interval_.count());
    thread_.sleep_until(timeout - diff);
    dp.emit();
  };
}

auto waybar::modules::Bluetooth::update() -> void {
  if (rfkill_.getState()) {
    status_ = "disabled";
  } else {
    status_ = "enabled";
  }

  label_.set_markup(
      fmt::format(
        format_,
        fmt::arg("status", status_),
        fmt::arg("icon", getIcon(0, status_))));

  if (tooltipEnabled()) {
    if (config_["tooltip-format"].isString()) {
      auto tooltip_format = config_["tooltip-format"].asString();
      auto tooltip_text = fmt::format(tooltip_format, status_);
      label_.set_tooltip_text(tooltip_text);
    } else {
      label_.set_tooltip_text(status_);
    }
  }
}
