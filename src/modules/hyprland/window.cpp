#include "modules/hyprland/window.hpp"

#include <spdlog/spdlog.h>

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

  auto replaceAll = [](std::string str, const std::string& from,
                       const std::string& to) -> std::string {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();
    }
    return str;
  };

  windowName = replaceAll(windowName, "&", "&amp;");

  if (windowName == lastView) return;

  lastView = windowName;

  spdlog::debug("hyprland window onevent with {}", windowName);

  dp.emit();
}

}  // namespace waybar::modules::hyprland