#include "modules/bluetooth.hpp"
#include "util/rfkill.hpp"
#include <linux/rfkill.h>

#include <time.h>

waybar::modules::Bluetooth::Bluetooth(const std::string& id, const Json::Value& config)
    : ALabel(config, "bluetooth", id, "{status}", 10),
      status_("disabled") {
  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::floor<std::chrono::seconds>(now + interval_);
    auto diff = std::chrono::seconds(timeout.time_since_epoch().count() % interval_.count());
    thread_.sleep_until(timeout - diff);
  };
  //dp.emit();
}

auto waybar::modules::Bluetooth::update() -> void {
  status_ = "enabled";
  if (waybar::util::rfkill::isDisabled(RFKILL_TYPE_BLUETOOTH)) {
    status_ = "disabled";
  } else {
    status_ = "enabled";
  }

  label_.set_markup(
      fmt::format(format_, fmt::arg("status", status_), fmt::arg("icon", getIcon(0, status_))));
  label_.get_style_context()->add_class(status_);

  //if (tooltipEnabled()) {
    //if (config_["tooltip-format"].isString()) {
      //auto tooltip_format = config_["tooltip-format"].asString();
      ////auto tooltip_text = fmt::format(tooltip_format, localtime);
      //label_.set_tooltip_text(tooltip_text);
    //} else {
      //label_.set_tooltip_text(text);
    //}
  //}
}
