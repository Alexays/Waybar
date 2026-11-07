#include "modules/pulseaudio.hpp"

#include "util/string.hpp"

waybar::modules::Pulseaudio::Pulseaudio(const std::string& id, const Json::Value& config)
    : ALabel(config, "pulseaudio", id, "{volume}%") {
  enableScroll();

  backend = util::AudioBackend::getInstance([this] { this->dp.emit(); });
  backend->setIgnoredSinks(config_["ignored-sinks"]);
  backend->setSinkMapping(config_["sink-mapping"]);

  if (config_["target"].isString() && config_["target"].asString() == "source") {
    target = util::PulseaudioTarget::Source;
  }
}

bool waybar::modules::Pulseaudio::handleScroll(const ScrollEvent& e) {
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

  backend->changeVolume(change_type, step, max_volume, target);
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
  for (auto const& port : ports) {
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
  auto sink_volume = backend->getSinkVolume();
  if (!alt_) {
    std::string format_name = "format";
    if (backend->isBluetooth()) {
      format_name = format_name + "-bluetooth";
      label_.add_css_class("bluetooth");
    } else {
      label_.remove_css_class("bluetooth");
    }
    if (backend->getSinkMuted()) {
      // Check muted bluetooth format exist, otherwise fallback to default muted format
      if (format_name != "format" && !config_[format_name + "-muted"].isString()) {
        format_name = "format";
      }
      format_name = format_name + "-muted";
      label_.add_css_class("muted");
      label_.add_css_class("sink-muted");
    } else {
      label_.remove_css_class("muted");
      label_.remove_css_class("sink-muted");
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
    label_.add_css_class("source-muted");
    if (config_["format-source-muted"].isString()) {
      format_source = config_["format-source-muted"].asString();
    }
  } else {
    label_.remove_css_class("source-muted");
    if (config_["format-source"].isString()) {
      format_source = config_["format-source"].asString();
    }
  }

  auto source_volume = backend->getSourceVolume();
  const std::string sink_desc =
      Glib::Markup::escape_text(toSingleLine(backend->getSinkDesc())).raw();
  const std::string source_desc =
      Glib::Markup::escape_text(toSingleLine(backend->getSourceDesc())).raw();

  format_source = fmt::format(fmt::runtime(format_source), fmt::arg("volume", source_volume),
                              fmt::arg("source_desc", source_desc));

  fmt::dynamic_format_arg_store<fmt::format_context> store;
  store.push_back(fmt::arg("desc", sink_desc));
  store.push_back(fmt::arg("volume", sink_volume));
  store.push_back(fmt::arg("format_source", format_source));
  store.push_back(fmt::arg("source_volume", source_volume));
  store.push_back(fmt::arg("source_desc", source_desc));
  store.push_back(fmt::arg("icon", getIcon(sink_volume, getPulseIcon())));

  auto text = fmt::vformat(format, store);
  if (text.empty()) {
    label_.set_visible(false);
  } else {
    setLabelMarkup(text);
    label_.set_visible(true);
  }

  if (tooltipEnabled()) {
    auto tooltip_format = resolveTooltipFormat("");
    if (!tooltip_format.empty()) {
      setTooltipMarkup(fmt::vformat(tooltip_format, store));
    } else {
      setTooltipMarkup(sink_desc);
    }
  }

  // Call parent update
  ALabel::update();
}
