#include "modules/hyprland/submap.hpp"

#include <spdlog/spdlog.h>

#include <util/sanitize_str.hpp>

namespace waybar::modules::hyprland {

Submap::Submap(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "submap", id, "{}", 0, true), bar_(bar) {
  modulesReady = true;

  if (!gIPC.get()) {
    gIPC = std::make_unique<IPC>();
  }

  label_.hide();
  ALabel::update();

  // register for hyprland ipc
  gIPC->registerForIPC("submap", this);
  dp.emit();
}

Submap::~Submap() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

auto Submap::update() -> void {
  std::lock_guard<std::mutex> lg(mutex_);

  if (submap_.empty()) {
    event_box_.hide();
  } else {
    label_.set_markup(fmt::format(fmt::runtime(format_), submap_));
    if (tooltipEnabled()) {
      label_.set_tooltip_text(submap_);
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

  auto submapName = ev.substr(ev.find_last_of('>') + 1);
  submapName = waybar::util::sanitize_string(submapName);

  submap_ = submapName;

  spdlog::debug("hyprland submap onevent with {}", submap_);

  dp.emit();
}
}  // namespace waybar::modules::hyprland
