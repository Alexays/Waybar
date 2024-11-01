#include "modules/pulseaudio_slider.hpp"

namespace waybar::modules {

PulseaudioSlider::PulseaudioSlider(const std::string& id, const Json::Value& config)
    : ASlider(config, "pulseaudio-slider", id) {
  backend = util::AudioBackend::getInstance([this] { this->dp.emit(); });
  backend->setIgnoredSinks(config_["ignored-sinks"]);

  if (config_["target"].isString()) {
    std::string target{config_["target"].asString()};
    if (target == "sink") {
      this->target = PulseaudioSliderTarget::Sink;
    } else if (target == "source") {
      this->target = PulseaudioSliderTarget::Source;
    }
  }
}

void PulseaudioSlider::update() {
  switch (target) {
    case PulseaudioSliderTarget::Sink:
      if (backend->getSinkMuted()) {
        scale_.set_value(min_);
      } else {
        scale_.set_value(backend->getSinkVolume());
      }
      break;

    case PulseaudioSliderTarget::Source:
      if (backend->getSourceMuted()) {
        scale_.set_value(min_);
      } else {
        scale_.set_value(backend->getSourceVolume());
      }
      break;
  }
}

void PulseaudioSlider::onValueChanged() {
  bool is_mute{false};

  switch (target) {
    case PulseaudioSliderTarget::Sink:
      if (backend->getSinkMuted()) {
        is_mute = true;
      }
      break;

    case PulseaudioSliderTarget::Source:
      if (backend->getSourceMuted()) {
        is_mute = true;
      }
      break;
  }

  uint16_t volume{scale_.get_value()};

  if (is_mute) {
    // Avoid setting sink/source to volume 0 if the user muted if via another mean.
    if (volume == 0) {
      return;
    }

    // If the sink/source is mute, but the user clicked the slider, unmute it!
    else {
      switch (target) {
        case PulseaudioSliderTarget::Sink:
          backend->toggleSinkMute(false);
          break;

        case PulseaudioSliderTarget::Source:
          backend->toggleSourceMute(false);
          break;
      }
    }
  }

  backend->changeVolume(volume, min_, max_);
}

}  // namespace waybar::modules
