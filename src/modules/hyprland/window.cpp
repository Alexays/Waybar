#include "modules/hyprland/window.hpp"

#include <spdlog/spdlog.h>

#include <regex>
#include <util/sanitize_str.hpp>

#include "modules/hyprland/backend.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/rewrite_title.hpp"

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
  gIPC->registerForIPC("activewindow", this);
}

Window::~Window() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

auto Window::update() -> void {
  // fix ampersands
  std::lock_guard<std::mutex> lg(mutex_);

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(
        fmt::format(format_, waybar::util::rewriteTitle(lastView, config_["rewrite"])));
  } else {
    label_.hide();
  }

  ALabel::update();
}

int Window::getActiveWorkspaceID(std::string monitorName) {
  auto cmd = waybar::util::command::exec("hyprctl monitors -j");
  assert(cmd.exit_code == 0);
  Json::Value json = parser_.parse(cmd.out);
  assert(json.isArray());
  auto monitor = std::find_if(json.begin(), json.end(),
                              [&](Json::Value monitor) { return monitor["name"] == monitorName; });
  if (monitor == std::end(json)) {
    return 0;
  }
  return (*monitor)["activeWorkspace"]["id"].as<int>();
}

std::string Window::getLastWindowTitle(int workspaceID) {
  auto cmd = waybar::util::command::exec("hyprctl workspaces -j");
  assert(cmd.exit_code == 0);
  Json::Value json = parser_.parse(cmd.out);
  assert(json.isArray());
  auto workspace = std::find_if(json.begin(), json.end(), [&](Json::Value workspace) {
    return workspace["id"].as<int>() == workspaceID;
  });

  if (workspace == std::end(json)) {
    return "";
  }
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

  windowName = waybar::util::sanitize_string(windowName);

  if (windowName == lastView) return;

  lastView = windowName;

  spdlog::debug("hyprland window onevent with {}", windowName);

  dp.emit();
}
}  // namespace waybar::modules::hyprland
