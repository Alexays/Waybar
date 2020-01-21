#include "modules/bluetooth.hpp"
#include "util/rfkill.hpp"
#include <linux/rfkill.h>

waybar::modules::Bluetooth::Bluetooth(const std::string& id, const Json::Value& config)
    : ALabel(config, "bluetooth", id, "{status}", 10) {
  dp.emit();
}

auto waybar::modules::Bluetooth::update() -> void {
  auto text = "enabled";
  if (waybar::util::isDisabled(RFKILL_TYPE_BLUETOOTH)) {
    text = "disabled";
  } else {
    text = "enabled";
  }

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
