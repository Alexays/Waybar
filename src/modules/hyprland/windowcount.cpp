#include "modules/hyprland/windowcount.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <vector>

#include "modules/hyprland/backend.hpp"
#include "util/sanitize_str.hpp"

namespace waybar::modules::hyprland {

WindowCount::WindowCount(const std::string& id, const Bar& bar, const Json::Value& config)
    : AAppIconLabel(config, "windowcount", id, "{count}", 0, true), bar_(bar) {
  modulesReady = true;
  separateOutputs_ =
      config.isMember("separate-outputs") ? config["separate-outputs"].asBool() : true;

  if (!gIPC) {
    gIPC = std::make_unique<IPC>();
  }

  queryActiveWorkspace();
  update();
  dp.emit();

  // register for hyprland ipc
  gIPC->registerForIPC("fullscreen", this);
  gIPC->registerForIPC("workspace", this);
  gIPC->registerForIPC("focusedmon", this);
  gIPC->registerForIPC("openwindow", this);
  gIPC->registerForIPC("closewindow", this);
  gIPC->registerForIPC("movewindow", this);
}

WindowCount::~WindowCount() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

auto WindowCount::update() -> void {
  std::lock_guard<std::mutex> lg(mutex_);

  std::string format = config_["format"].asString();
  std::string formatEmpty = config_["format-empty"].asString();
  std::string formatWindowed = config_["format-windowed"].asString();
  std::string formatFullscreen = config_["format-fullscreen"].asString();

  setClass("empty", workspace_.windows == 0);
  setClass("fullscreen", workspace_.hasfullscreen);

  if (workspace_.windows == 0 && !formatEmpty.empty()) {
    label_.set_markup(fmt::format(fmt::runtime(formatEmpty), workspace_.windows));
  } else if (!workspace_.hasfullscreen && !formatWindowed.empty()) {
    label_.set_markup(fmt::format(fmt::runtime(formatWindowed), workspace_.windows));
  } else if (workspace_.hasfullscreen && !formatFullscreen.empty()) {
    label_.set_markup(fmt::format(fmt::runtime(formatFullscreen), workspace_.windows));
  } else if (!format.empty()) {
    label_.set_markup(fmt::format(fmt::runtime(format), workspace_.windows));
  } else {
    label_.set_text(fmt::format("{}", workspace_.windows));
  }

  label_.show();
  AAppIconLabel::update();
}

auto WindowCount::getActiveWorkspace() -> Workspace {
  const auto workspace = gIPC->getSocket1JsonReply("activeworkspace");

  if (workspace.isObject()) {
    return Workspace::parse(workspace);
  }

  return {};
}

auto WindowCount::getActiveWorkspace(const std::string& monitorName) -> Workspace {
  const auto monitors = gIPC->getSocket1JsonReply("monitors");
  if (monitors.isArray()) {
    auto monitor = std::find_if(monitors.begin(), monitors.end(), [&](Json::Value monitor) {
      return monitor["name"] == monitorName;
    });
    if (monitor == std::end(monitors)) {
      spdlog::warn("Monitor not found: {}", monitorName);
      return Workspace{-1, 0, false};
    }
    const int id = (*monitor)["activeWorkspace"]["id"].asInt();

    const auto workspaces = gIPC->getSocket1JsonReply("workspaces");
    if (workspaces.isArray()) {
      auto workspace = std::find_if(workspaces.begin(), workspaces.end(),
                                    [&](Json::Value workspace) { return workspace["id"] == id; });
      if (workspace == std::end(workspaces)) {
        spdlog::warn("No workspace with id {}", id);
        return Workspace{-1, 0, false};
      }
      return Workspace::parse(*workspace);
    };
  };

  return {};
}

auto WindowCount::Workspace::parse(const Json::Value& value) -> WindowCount::Workspace {
  return Workspace{
      value["id"].asInt(),
      value["windows"].asInt(),
      value["hasfullscreen"].asBool(),
  };
}

void WindowCount::queryActiveWorkspace() {
  std::lock_guard<std::mutex> lg(mutex_);

  if (separateOutputs_) {
    workspace_ = getActiveWorkspace(this->bar_.output->name);
  } else {
    workspace_ = getActiveWorkspace();
  }
}

void WindowCount::onEvent(const std::string& ev) {
  queryActiveWorkspace();
  dp.emit();
}

void WindowCount::setClass(const std::string& classname, bool enable) {
  if (enable) {
    if (!bar_.window.get_style_context()->has_class(classname)) {
      bar_.window.get_style_context()->add_class(classname);
    }
  } else {
    bar_.window.get_style_context()->remove_class(classname);
  }
}

}  // namespace waybar::modules::hyprland
