#include "modules/hyprland/window.hpp"

#include <spdlog/spdlog.h>

#include <util/sanitize_str.hpp>

#include "modules/hyprland/backend.hpp"

namespace waybar::modules::hyprland {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "window", id, "{}", 0, true), bar_(bar) {
  modulesReady = true;

  if (!gIPC.get()) {
    gIPC = std::make_unique<IPC>();
  }

  label_.hide();
  ALabel::update();

  // register for hyprland ipc
  gIPC->registerForIPC("activewindow", [&](const std::string& ev) { this->onEvent(ev); });
}

auto Window::update() -> void {
  // fix ampersands
  std::lock_guard<std::mutex> lg(mutex_);

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(fmt::format(format_, lastView));
  } else {
    label_.hide();
  }

  ALabel::update();
}

void Window::onEvent(const std::string& ev) {
  std::lock_guard<std::mutex> lg(mutex_);
  auto windowName = ev.substr(ev.find_first_of(',') + 1).substr(0, 256);

  windowName = waybar::util::sanitize_string(windowName);

  if (windowName == lastView) return;

  lastView = windowName;

  spdlog::debug("hyprland window onevent with {}", windowName);

  dp.emit();
}

}  // namespace waybar::modules::hyprland