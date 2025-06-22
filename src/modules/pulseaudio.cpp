#include "modules/pulseaudio.hpp"

waybar::modules::Pulseaudio::Pulseaudio(const std::string &id, const Json::Value &config)
    : ALabel(config, "pulseaudio", id, "{volume}%") {
  event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &Pulseaudio::handleScroll));

  backend = util::AudioBackend::getInstance([this] { this->dp.emit(); });
  backend->setIgnoredSinks(config_["ignored-sinks"]);
}

bool waybar::modules::Pulseaudio::handleScroll(GdkEventScroll *e) {
  // change the pulse volume only when no user provided
  // events are configured
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScroll(e);
  }
  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }
  int max_volume = 100;
  double step = 1;
  // isDouble returns true for integers as well, just in case
  if (config_["scroll-step"].isDouble()) {
    step = config_["scroll-step"].asDouble();
  }
  if (config_["max-volume"].isInt()) {
    max_volume = config_["max-volume"].asInt();
  }

  auto change_type = (dir == SCROLL_DIR::UP || dir == SCROLL_DIR::RIGHT)
                         ? util::ChangeType::Increase
                         : util::ChangeType::Decrease;

  backend->changeVolume(change_type, step, max_volume);
  return true;
}

static const std::array<std::string, 9> ports = {
    "headphone", "speaker", "hdmi", "headset", "hands-free", "portable", "car", "hifi", "phone",
};

const std::vector<std::string> waybar::modules::Pulseaudio::getPulseIcon() const {
  std::vector<std::string> res;
  auto sink_muted = backend->getSinkMuted();
  if (sink_muted) {
    res.emplace_back(backend->getCurrentSinkName() + "-muted");
  }
  res.push_back(backend->getCurrentSinkName());
  res.push_back(backend->getDefaultSourceName());
  std::string nameLC = backend->getSinkPortName() + backend->getFormFactor();
  std::transform(nameLC.begin(), nameLC.end(), nameLC.begin(), ::tolower);
  for (auto const &port : ports) {
    if (nameLC.find(port) != std::string::npos) {
      if (sink_muted) {
        res.emplace_back(port + "-muted");
      }
      res.push_back(port);
      break;
    }
  }
  if (sink_muted) {
    res.emplace_back("default-muted");
  }
  return res;
}

auto waybar::modules::Pulseaudio::update() -> void {
  auto format = format_;
  std::string tooltip_format;
  auto sink_volume = backend->getSinkVolume();
  if (!alt_) {
    std::string format_name = "format";
    if (backend->isBluetooth()) {
      format_name = format_name + "-bluetooth";
      label_.get_style_context()->add_class("bluetooth");
    } else {
      label_.get_style_context()->remove_class("bluetooth");
    }
    if (backend->getSinkMuted()) {
      // Check muted bluetooth format exist, otherwise fallback to default muted format
      if (format_name != "format" && !config_[format_name + "-muted"].isString()) {
        format_name = "format";
      }
      format_name = format_name + "-muted";
      label_.get_style_context()->add_class("muted");
      label_.get_style_context()->add_class("sink-muted");
    } else {
      label_.get_style_context()->remove_class("muted");
      label_.get_style_context()->remove_class("sink-muted");
    }
    auto state = getState(sink_volume, true);
    if (!state.empty() && config_[format_name + "-" + state].isString()) {
      format = config_[format_name + "-" + state].asString();
    } else if (config_[format_name].isString()) {
      format = config_[format_name].asString();
    }
  }
  // TODO: find a better way to split source/sink
  std::string format_source = "{volume}%";
  if (backend->getSourceMuted()) {
    label_.get_style_context()->add_class("source-muted");
    if (config_["format-source-muted"].isString()) {
      format_source = config_["format-source-muted"].asString();
    }
  } else {
    label_.get_style_context()->remove_class("source-muted");
    if (config_["format-source"].isString()) {
      format_source = config_["format-source"].asString();
    }
  }

  auto source_volume = backend->getSourceVolume();
  auto sink_desc = backend->getSinkDesc();
  auto source_desc = backend->getSourceDesc();

  format_source = fmt::format(fmt::runtime(format_source), fmt::arg("volume", source_volume));
  auto text = fmt::format(
      fmt::runtime(format), fmt::arg("desc", sink_desc), fmt::arg("volume", sink_volume),
      fmt::arg("format_source", format_source), fmt::arg("source_volume", source_volume),
      fmt::arg("source_desc", source_desc), fmt::arg("icon", getIcon(sink_volume, getPulseIcon())));
  if (text.empty()) {
    label_.hide();
  } else {
    label_.set_markup(text);
    label_.show();
  }

  if (tooltipEnabled()) {
    if (tooltip_format.empty() && config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    if (!tooltip_format.empty()) {
      label_.set_tooltip_text(fmt::format(
          fmt::runtime(tooltip_format), fmt::arg("desc", sink_desc),
          fmt::arg("volume", sink_volume), fmt::arg("format_source", format_source),
          fmt::arg("source_volume", source_volume), fmt::arg("source_desc", source_desc),
          fmt::arg("icon", getIcon(sink_volume, getPulseIcon()))));
    } else {
      label_.set_tooltip_text(sink_desc);
    }
  }

  // Call parent update
  ALabel::update();
}
