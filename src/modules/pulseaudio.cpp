#include "modules/pulseaudio.hpp"

namespace waybar::modules {

Pulseaudio::Pulseaudio(const std::string &id, const Json::Value &config)
    : ALabel(config, "pulseaudio", id, "{volume}%") {
  event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &Pulseaudio::handleScrollEvent));

  backend = util::AudioBackend::getInstance([this] { dp.emit(); });
  backend->setIgnoredSinks(config_["ignored-sinks"]);
}

bool Pulseaudio::handleScrollEvent(GdkEventScroll *e) {
  // change the pulse volume only when no user provided
  // events are configured
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScrollEvent(e);
  }

  auto direction = AModule::getScrollDir(e);
  if (direction == SCROLL_DIR::NONE) return true;

  const int maxVolume = config_["max-volume"].isInt() ? config_["max-volume"].asInt() : 100;
  const double scrollStep =
      config_["scroll-step"].isDouble() ? config_["scroll-step"].asDouble() : 1.0;

  auto change_type = (direction == SCROLL_DIR::UP || direction == SCROLL_DIR::RIGHT)
                         ? util::ChangeType::Increase
                         : util::ChangeType::Decrease;

  backend->changeVolume(change_type, scrollStep, maxVolume);
  return true;
}

static constexpr std::array<std::string_view, 9> audioPorts = {
    "headphone", "speaker", "hdmi", "headset", "hands-free", "portable", "car", "hifi", "phone",
};

std::vector<std::string> Pulseaudio::getPulseIcon() const {
  std::vector<std::string> icons;
  const auto isMuted = backend->getSinkMuted();
  const auto &sinkName = backend->getCurrentSinkName();
  const auto &sourceName = backend->getDefaultSourceName();
  const auto portName = backend->getSinkPortName();
  const auto formFactor = backend->getFormFactor();

  if (isMuted) icons.emplace_back(sinkName + "-muted");
  icons.emplace_back(sinkName);
  icons.emplace_back(sourceName);

  std::string portComposite = portName + formFactor;
  std::transform(portComposite.begin(), portComposite.end(), portComposite.begin(), ::tolower);

  for (auto const &port : audioPorts) {
    if (portComposite.find(port) != std::string::npos) {
      if (isMuted) icons.emplace_back(std::string(port) + "-muted");
      icons.emplace_back(std::string(port));
      break;
    }
  }

  if (isMuted) icons.emplace_back("default-muted");
  return icons;
}

auto Pulseaudio::update() -> void {
  auto format = format_;
  std::string sourceFormat = "{volume}%";

  const auto sinkVolume = backend->getSinkVolume();
  const auto sourceVolume = backend->getSourceVolume();
  const auto sinkDesc = backend->getSinkDesc();
  const auto sourceDesc = backend->getSourceDesc();

  if (!alt_) {
    std::string formatKey = "format";
    if (backend->isBluetooth()) {
      formatKey += "-bluetooth";
      label_.get_style_context()->add_class("bluetooth");
    } else {
      label_.get_style_context()->remove_class("bluetooth");
    }

    if (backend->getSinkMuted()) {
      // Check muted bluetooth format exist, otherwise fallback to default muted format
      if (formatKey != "format" && !config_[formatKey + "-muted"].isString()) {
        formatKey = "format";
      }
      formatKey += "-muted";
      label_.get_style_context()->add_class("muted");
      label_.get_style_context()->add_class("sink-muted");
    } else {
      label_.get_style_context()->remove_class("muted");
      label_.get_style_context()->remove_class("sink-muted");
    }

    const auto state = getState(sinkVolume, true);
    if (!state.empty() && config_[formatKey + "-" + state].isString()) {
      format = config_[formatKey + "-" + state].asString();
    } else if (config_[formatKey].isString()) {
      format = config_[formatKey].asString();
    }
  }
  // TODO: find a better way to split source/sink

  if (backend->getSourceMuted()) {
    label_.get_style_context()->add_class("source-muted");
    if (config_["format-source-muted"].isString()) {
      sourceFormat = config_["format-source-muted"].asString();
    }
  } else {
    label_.get_style_context()->remove_class("source-muted");
    if (config_["format-source"].isString()) {
      sourceFormat = config_["format-source"].asString();
    }
  }

  sourceFormat = fmt::format(fmt::runtime(sourceFormat), fmt::arg("volume", sourceVolume));

  const auto text = fmt::format(
      fmt::runtime(format), fmt::arg("desc", sinkDesc), fmt::arg("volume", sinkVolume),
      fmt::arg("format_source", sourceFormat), fmt::arg("source_volume", sourceVolume),
      fmt::arg("source_desc", sourceDesc), fmt::arg("icon", getIcon(sinkVolume, getPulseIcon())));

  if (text.empty()) {
    label_.hide();
  } else {
    label_.set_markup(text);
    label_.show();
  }

  if (tooltipEnabled()) {
    std::string tooltipFmt;
    if (config_["tooltip-format"].isString()) {
      tooltipFmt = config_["tooltip-format"].asString();
    }

    if (!tooltipFmt.empty()) {
      label_.set_tooltip_markup(
          fmt::format(fmt::runtime(tooltipFmt), fmt::arg("desc", sinkDesc),
                      fmt::arg("volume", sinkVolume), fmt::arg("format_source", sourceFormat),
                      fmt::arg("source_volume", sourceVolume), fmt::arg("source_desc", sourceDesc),
                      fmt::arg("icon", getIcon(sinkVolume, getPulseIcon()))));
    } else {
      label_.set_tooltip_text(sinkDesc);
    }
  }

  // Call parent update
  ALabel::update();
}

}  // namespace waybar::modules
