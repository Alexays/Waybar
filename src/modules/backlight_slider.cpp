#include "modules/backlight_slider.hpp"

#include "ASlider.hpp"

namespace waybar::modules {

BacklightSlider::BacklightSlider(const std::string& id, const Json::Value& config)
    : ASlider(config, "backlight-slider", id, 1),
      preferred_device_(config["device"].isString() ? config["device"].asString() : ""),
      backend(interval_, [this] { this->dp.emit(); }) {}

void BacklightSlider::update() {
  int brightness = backend.get_scaled_brightness(preferred_device_);
  setValueSilently(brightness);
}

void BacklightSlider::onCommit(int value) {
  backend.set_scaled_brightness(preferred_device_, value);
}

}  // namespace waybar::modules