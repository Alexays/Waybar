#include "modules/backlight_slider.hpp"

#include "ASlider.hpp"

namespace waybar::modules {

BacklightSlider::BacklightSlider(const std::string& id, const Json::Value& config)
    : ASlider(config, "backlight-slider", id),
      interval_(config_["interval"].isUInt() ? config_["interval"].asUInt() : 1000),
      preferred_device_(config["device"].isString() ? config["device"].asString() : ""),
      backend(interval_, [this] { this->dp.emit(); }) {}

void BacklightSlider::update() {
  uint16_t brightness = backend.get_scaled_brightness(preferred_device_);
  scale_.set_value(brightness);
}

void BacklightSlider::onValueChanged() {
  auto brightness = scale_.get_value();
  backend.set_scaled_brightness(preferred_device_, brightness);
}

}  // namespace waybar::modules