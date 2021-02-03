#include "modules/bluetooth.hpp"

#include <fmt/format.h>

waybar::modules::Bluetooth::Bluetooth(const std::string& id, const Json::Value& config)
    : ALabel(config, "bluetooth", id, "{icon}", 10), rfkill_{RFKILL_TYPE_BLUETOOTH} {
  rfkill_.on_update.connect(sigc::hide(sigc::mem_fun(*this, &Bluetooth::update)));
}

auto waybar::modules::Bluetooth::update() -> void {
  std::string status = rfkill_.getState() ? "disabled" : "enabled";

  label_.set_markup(
      fmt::format(format_, fmt::arg("status", status), fmt::arg("icon", getIcon(0, status))));

  if (tooltipEnabled()) {
    if (config_["tooltip-format"].isString()) {
      auto tooltip_format = config_["tooltip-format"].asString();
      auto tooltip_text = fmt::format(tooltip_format, status);
      label_.set_tooltip_text(tooltip_text);
    } else {
      label_.set_tooltip_text(status);
    }
  }
}
