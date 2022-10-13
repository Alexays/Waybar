#include "modules/hyprland/window.hpp"

#include <spdlog/spdlog.h>

#include "modules/hyprland/backend.hpp"
#include "util/command.hpp"
#include "util/json.hpp"

namespace waybar::modules::hyprland {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "window", id, "{}", 0, true), bar_(bar) {
  modulesReady = true;
  separate_outputs = config["separate-outputs"].as<bool>();

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

uint Window::getActiveWorkspaceID(std::string monitorName) {
  auto cmd = waybar::util::command::exec("hyprctl monitors -j");
  assert(cmd.exit_code == 0);
  Json::Value json = parser_.parse(cmd.out);
  assert(json.isArray());
  auto monitor = std::find_if(json.begin(), json.end(), [&](Json::Value monitor){
    return monitor["name"] == monitorName;
  });
  assert(monitor != std::end(json));
  return (*monitor)["activeWorkspace"]["id"].as<uint>();
}

std::string Window::getLastWindowTitle(uint workspaceID) {
  auto cmd = waybar::util::command::exec("hyprctl workspaces -j");
  assert(cmd.exit_code == 0);
  Json::Value json = parser_.parse(cmd.out);
  assert(json.isArray());
  auto workspace = std::find_if(json.begin(), json.end(), [&](Json::Value workspace){
    return workspace["id"].as<uint>() == workspaceID;
  });
  assert(workspace != std::end(json));
  return (*workspace)["lastwindowtitle"].as<std::string>();
}

void Window::onEvent(const std::string& ev) {
  std::lock_guard<std::mutex> lg(mutex_);

  std::string windowName;
  if (separate_outputs) {
    windowName = getLastWindowTitle(getActiveWorkspaceID(this->bar_.output->name));
  } else {
    windowName = ev.substr(ev.find_first_of(',') + 1).substr(0, 256);
  }

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