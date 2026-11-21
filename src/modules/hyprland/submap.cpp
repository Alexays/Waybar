#include "modules/hyprland/submap.hpp"

#include <spdlog/spdlog.h>

#include "util/string.hpp"

namespace waybar::modules::hyprland {

Submap::Submap(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "submap", id, "{}", 0, true), bar_(bar), m_ipc(IPC::inst()) {
  parseConfig(config);

  label_.hide();
  ALabel::update();

  if (config["icons"].isObject()) {
    const Json::Value& icons = config["icons"];

    for (const std::string& key : icons.getMemberNames()) {
      const Json::Value& value = icons[key];

      if (value.isString()) {
        icons_[key] = value.asString();
      }
    }
  }

  // get the active submap when open
  initSubmap();

  // register for hyprland ipc
  m_ipc.registerForIPC("submap", this);
}

Submap::~Submap() {
  m_ipc.unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

auto Submap::parseConfig(const Json::Value& config) -> void {
  auto const& alwaysOn = config["always-on"];
  if (alwaysOn.isBool()) {
    always_on_ = alwaysOn.asBool();
  }

  auto const& defaultSubmap = config["default-submap"];
  if (defaultSubmap.isString()) {
    default_submap_ = defaultSubmap.asString();
  }
}

auto Submap::update() -> void {
  std::lock_guard<std::mutex> lg(mutex_);

  // Handle style class changes
  if (!prev_submap_.empty()) {
    label_.get_style_context()->remove_class(prev_submap_);
  }

  if (!submap_.empty()) {
    label_.get_style_context()->add_class(submap_);
  }

  prev_submap_ = submap_;

  if (submap_.empty()) {
    event_box_.hide();
  } else {
    label_.set_markup(
        fmt::format(fmt::runtime(format_), fmt::arg("submap", submap_), fmt::arg("icon", icon_)));
    if (tooltipEnabled()) {
      setTooltipMarkup(submap_);
    }
    event_box_.show();
  }
  // Call parent update
  ALabel::update();
}

void Submap::onEvent(const std::string& ev) {
  std::lock_guard<std::mutex> lg(mutex_);

  if (ev.find("submap") == std::string::npos) {
    return;
  }

  const auto separator = ev.find(">>");
  if (separator == std::string::npos) {
    spdlog::warn("hyprland submap received malformed event: {}", ev);
    return;
  }
  setSubmap(ev.substr(separator + 2));

  spdlog::debug("hyprland submap onevent with {}", submap_);

  dp.emit();
}

void Submap::onReconnect() {
  std::lock_guard<std::mutex> lg(mutex_);
  initSubmap();
}

void Submap::initSubmap() {
  // Events only announce a change, so a module created inside a submap has to ask for it (#4736)
  std::string name;
  try {
    name = trim(m_ipc.getSocket1Reply("submap"));
  } catch (const std::exception& e) {
    spdlog::warn("hyprland submap: could not read the active submap: {}", e.what());
  }

  // Not in a submap, or a Hyprland too old to know the request
  if (name == "default" || name == "unknown request") {
    name.clear();
  }

  setSubmap(name);

  spdlog::debug("hyprland submap initSubmap found {}", submap_);

  dp.emit();
}

void Submap::setSubmap(const std::string& name) {
  submap_ = name;

  if (!icons_[submap_].empty()) {
    icon_ = icons_[submap_];
  } else {
    icon_ = "";
  }

  if (submap_.empty() && always_on_) {
    submap_ = default_submap_;
  }
}
}  // namespace waybar::modules::hyprland
