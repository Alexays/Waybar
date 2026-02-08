#include "modules/hyprland/windowcount.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <vector>

#include "modules/hyprland/backend.hpp"

namespace waybar::modules::hyprland {

WindowCount::WindowCount(const std::string& id, const Bar& bar, const Json::Value& config,
                         std::mutex& reap_mtx, std::list<pid_t>& reap)
    : AAppIconLabel(config, "windowcount", id, "{count}", reap_mtx, reap, 0, true),
      bar_(bar),
      m_ipc(IPC::inst()) {
  separateOutputs_ =
      config.isMember("separate-outputs") ? config["separate-outputs"].asBool() : true;

  queryActiveWorkspace();
  update();
  dp.emit();

  // register for hyprland ipc
  m_ipc.registerForIPC("fullscreen", this);
  m_ipc.registerForIPC("workspace", this);
  m_ipc.registerForIPC("focusedmon", this);
  m_ipc.registerForIPC("openwindow", this);
  m_ipc.registerForIPC("closewindow", this);
  m_ipc.registerForIPC("movewindow", this);
}

WindowCount::~WindowCount() {
  m_ipc.unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

auto WindowCount::update() -> void {
  std::lock_guard<std::mutex> lg(mutex_);
  
  queryActiveWorkspace();

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
  const auto workspace = m_ipc.getSocket1JsonReply("activeworkspace");

  if (workspace.isObject()) {
    return Workspace::parse(workspace);
  }

  return {};
}

auto WindowCount::getActiveWorkspace(const std::string& monitorName) -> Workspace {
  const auto monitors = m_ipc.getSocket1JsonReply("monitors");
  if (monitors.isArray()) {
    auto monitor = std::ranges::find_if(
        monitors, [&](Json::Value monitor) { return monitor["name"] == monitorName; });
    if (monitor == std::end(monitors)) {
      spdlog::warn("Monitor not found: {}", monitorName);
      return Workspace{
          .id = -1,
          .windows = 0,
          .hasfullscreen = false,
      };
    }
    const int id = (*monitor)["activeWorkspace"]["id"].asInt();

    const auto workspaces = m_ipc.getSocket1JsonReply("workspaces");
    if (workspaces.isArray()) {
      auto workspace = std::ranges::find_if(
          workspaces, [&](Json::Value workspace) { return workspace["id"] == id; });
      if (workspace == std::end(workspaces)) {
        spdlog::warn("No workspace with id {}", id);
        return Workspace{
            .id = -1,
            .windows = 0,
            .hasfullscreen = false,
        };
      }
      return Workspace::parse(*workspace);
    };
  };

  return {};
}

auto WindowCount::Workspace::parse(const Json::Value& value) -> WindowCount::Workspace {
  return Workspace{
      .id = value["id"].asInt(),
      .windows = value["windows"].asInt(),
      .hasfullscreen = value["hasfullscreen"].asBool(),
  };
}

void WindowCount::queryActiveWorkspace() {
  if (separateOutputs_) {
    workspace_ = getActiveWorkspace(this->bar_.output->name);
  } else {
    workspace_ = getActiveWorkspace();
  }
}

void WindowCount::onEvent(const std::string& ev) {
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
