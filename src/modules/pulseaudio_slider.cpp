#include "modules/pulseaudio_slider.hpp"

namespace waybar::modules {

PulseaudioSlider::PulseaudioSlider(const std::string& id, const Json::Value& config)
    : ASlider(config, "pulseaudio-slider", id) {
  backend = util::AudioBackend::getInstance([this] { this->dp.emit(); });
  backend->setIgnoredSinks(config_["ignored-sinks"]);
  backend->setSinkMapping(config_["sink-mapping"]);

  if (config_["target"].isString()) {
    std::string target = config_["target"].asString();
    if (target == "sink") {
      this->target = util::PulseaudioTarget::Sink;
    } else if (target == "source") {
      this->target = util::PulseaudioTarget::Source;
    }
  }

  if (config_["zero-on-mute"].isBool()) {
    zero_on_mute = config_["zero-on-mute"].asBool();
  }

  if (config_["unmute-on-volume-change"].isBool()) {
    unmute_on_volume_change = config_["unmute-on-volume-change"].asBool();
  }
}

void PulseaudioSlider::update() {
  uint16_t display_value = backend->getVolume(target);
  bool is_muted = backend->getMuted(target);

  if (is_muted) {
    if (zero_on_mute) {
      display_value = min_;
    }
    if (!previously_muted) {
      scale_.get_style_context()->add_class("muted");
    }
  } else if (previously_muted) {
    scale_.get_style_context()->remove_class("muted");
  }

  scale_.set_value(display_value);

  previously_muted = is_muted;
}

void PulseaudioSlider::onValueChanged() {
  uint16_t slider_value = scale_.get_value();

  // Avoid setting sink/source to volume 0 if the user muted it via other means.
  if (!backend->getMuted(target) || slider_value != 0) {
    if (unmute_on_volume_change) {
      backend->unmute(target);
    }
    backend->changeVolume(slider_value, min_, max_, target);
  }
}

}  // namespace waybar::modules
