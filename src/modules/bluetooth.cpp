#include "modules/bluetooth.hpp"
#include "util/rfkill.hpp"
#include <linux/rfkill.h>
#include <time.h>

#include <iostream>

waybar::modules::Bluetooth::Bluetooth(const std::string& id, const Json::Value& config)
    : ALabel(config, "bluetooth", id, "{status}", 10),
      status_("disabled"),
      rfkill_(*(new waybar::util::Rfkill(RFKILL_TYPE_BLUETOOTH))) {
  thread_ = [this] {
    dp.emit();
    rfkill_.waitForEvent();
  };
}

auto waybar::modules::Bluetooth::update() -> void {
  status_ = "enabled";
  if (rfkill_.getState()) {
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
      //label_.set_tooltip_text(status_);
    //}
  //}
}
