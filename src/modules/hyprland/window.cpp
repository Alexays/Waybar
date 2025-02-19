#include "modules/hyprland/window.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <shared_mutex>
#include <vector>

#include "modules/hyprland/backend.hpp"
#include "util/rewrite_string.hpp"
#include "util/sanitize_str.hpp"

namespace waybar::modules::hyprland {

std::shared_mutex windowIpcSmtx;

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AAppIconLabel(config, "window", id, "{title}", 0, true), bar_(bar), m_ipc(IPC::inst()) {
  std::unique_lock<std::shared_mutex> windowIpcUniqueLock(windowIpcSmtx);

  modulesReady = true;
  separateOutputs_ = config["separate-outputs"].asBool();

  // register for hyprland ipc
  m_ipc.registerForIPC("activewindow", this);
  m_ipc.registerForIPC("closewindow", this);
  m_ipc.registerForIPC("movewindow", this);
  m_ipc.registerForIPC("changefloatingmode", this);
  m_ipc.registerForIPC("fullscreen", this);

  windowIpcUniqueLock.unlock();

  queryActiveWorkspace();
  update();
  dp.emit();
}

Window::~Window() {
  std::unique_lock<std::shared_mutex> windowIpcUniqueLock(windowIpcSmtx);
  m_ipc.unregisterForIPC(this);
}

auto Window::update() -> void {
  std::shared_lock<std::shared_mutex> windowIpcShareLock(windowIpcSmtx);

  std::string windowName = waybar::util::sanitize_string(workspace_.last_window_title);
  std::string windowAddress = workspace_.last_window;

  windowData_.title = windowName;

  std::string label_text;
  if (!format_.empty()) {
    label_.show();
    label_text = waybar::util::rewriteString(
        fmt::format(fmt::runtime(format_), fmt::arg("title", windowName),
                    fmt::arg("initialTitle", windowData_.initial_title),
                    fmt::arg("class", windowData_.class_name),
                    fmt::arg("initialClass", windowData_.initial_class_name)),
        config_["rewrite"]);
    label_.set_markup(label_text);
  } else {
    label_.hide();
  }

  if (tooltipEnabled()) {
    std::string tooltip_format;
    if (config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    if (!tooltip_format.empty()) {
      label_.set_tooltip_text(
          fmt::format(fmt::runtime(tooltip_format), fmt::arg("title", windowName),
                      fmt::arg("initialTitle", windowData_.initial_title),
                      fmt::arg("class", windowData_.class_name),
                      fmt::arg("initialClass", windowData_.initial_class_name)));
    } else if (!label_text.empty()) {
      label_.set_tooltip_text(label_text);
    }
  }

  if (focused_) {
    image_.show();
  } else {
    image_.hide();
  }

  setClass("empty", workspace_.windows == 0);
  setClass("solo", solo_);
  setClass("floating", allFloating_);
  setClass("swallowing", swallowing_);
  setClass("fullscreen", fullscreen_);

  if (!lastSoloClass_.empty() && soloClass_ != lastSoloClass_) {
    if (bar_.window.get_style_context()->has_class(lastSoloClass_)) {
      bar_.window.get_style_context()->remove_class(lastSoloClass_);
      spdlog::trace("Removing solo class: {}", lastSoloClass_);
    }
  }

  if (!soloClass_.empty() && soloClass_ != lastSoloClass_) {
    bar_.window.get_style_context()->add_class(soloClass_);
    spdlog::trace("Adding solo class: {}", soloClass_);
  }
  lastSoloClass_ = soloClass_;

  AAppIconLabel::update();
}

auto Window::getActiveWorkspace() -> Workspace {
  const auto workspace = IPC::inst().getSocket1JsonReply("activeworkspace");

  if (workspace.isObject()) {
    return Workspace::parse(workspace);
  }

  return {};
}

auto Window::getActiveWorkspace(const std::string& monitorName) -> Workspace {
  const auto monitors = IPC::inst().getSocket1JsonReply("monitors");
  if (monitors.isArray()) {
    auto monitor = std::ranges::find_if(
        monitors, [&](Json::Value monitor) { return monitor["name"] == monitorName; });
    if (monitor == std::end(monitors)) {
      spdlog::warn("Monitor not found: {}", monitorName);
      return Workspace{
          .id = -1,
          .windows = 0,
          .last_window = "",
          .last_window_title = "",
      };
    }
    const int id = (*monitor)["activeWorkspace"]["id"].asInt();

    const auto workspaces = IPC::inst().getSocket1JsonReply("workspaces");
    if (workspaces.isArray()) {
      auto workspace = std::ranges::find_if(
          workspaces, [&](Json::Value workspace) { return workspace["id"] == id; });
      if (workspace == std::end(workspaces)) {
        spdlog::warn("No workspace with id {}", id);
        return Workspace{
            .id = -1,
            .windows = 0,
            .last_window = "",
            .last_window_title = "",
        };
      }
      return Workspace::parse(*workspace);
    };
  };

  return {};
}

auto Window::Workspace::parse(const Json::Value& value) -> Window::Workspace {
  return Workspace{
      .id = value["id"].asInt(),
      .windows = value["windows"].asInt(),
      .last_window = value["lastwindow"].asString(),
      .last_window_title = value["lastwindowtitle"].asString(),
  };
}

auto Window::WindowData::parse(const Json::Value& value) -> Window::WindowData {
  return WindowData{.floating = value["floating"].asBool(),
                    .monitor = value["monitor"].asInt(),
                    .class_name = value["class"].asString(),
                    .initial_class_name = value["initialClass"].asString(),
                    .title = value["title"].asString(),
                    .initial_title = value["initialTitle"].asString(),
                    .fullscreen = value["fullscreen"].asBool(),
                    .grouped = !value["grouped"].empty()};
}

void Window::queryActiveWorkspace() {
  std::shared_lock<std::shared_mutex> windowIpcShareLock(windowIpcSmtx);

  if (separateOutputs_) {
    workspace_ = getActiveWorkspace(this->bar_.output->name);
  } else {
    workspace_ = getActiveWorkspace();
  }

  focused_ = true;
  if (workspace_.windows > 0) {
    const auto clients = m_ipc.getSocket1JsonReply("clients");
    if (clients.isArray()) {
      auto activeWindow = std::ranges::find_if(
          clients, [&](Json::Value window) { return window["address"] == workspace_.last_window; });

      if (activeWindow == std::end(clients)) {
        focused_ = false;
        return;
      }

      windowData_ = WindowData::parse(*activeWindow);
      updateAppIconName(windowData_.class_name, windowData_.initial_class_name);
      std::vector<Json::Value> workspaceWindows;
      std::ranges::copy_if(clients, std::back_inserter(workspaceWindows), [&](Json::Value window) {
        return window["workspace"]["id"] == workspace_.id && window["mapped"].asBool();
      });
      swallowing_ = std::ranges::any_of(workspaceWindows, [&](Json::Value window) {
        return !window["swallowing"].isNull() && window["swallowing"].asString() != "0x0";
      });
      std::vector<Json::Value> visibleWindows;
      std::ranges::copy_if(workspaceWindows, std::back_inserter(visibleWindows),
                           [&](Json::Value window) { return !window["hidden"].asBool(); });
      solo_ = 1 == std::count_if(visibleWindows.begin(), visibleWindows.end(),
                                 [&](Json::Value window) { return !window["floating"].asBool(); });
      allFloating_ = std::ranges::all_of(
          visibleWindows, [&](Json::Value window) { return window["floating"].asBool(); });
      fullscreen_ = windowData_.fullscreen;

      // Fullscreen windows look like they are solo
      if (fullscreen_) {
        solo_ = true;
      }

      if (solo_) {
        soloClass_ = windowData_.class_name;
      } else {
        soloClass_ = "";
      }
    }
  } else {
    focused_ = false;
    windowData_ = WindowData{};
    allFloating_ = false;
    swallowing_ = false;
    fullscreen_ = false;
    solo_ = false;
    soloClass_ = "";
  }
}

void Window::onEvent(const std::string& ev) {
  queryActiveWorkspace();

  dp.emit();
}

void Window::setClass(const std::string& classname, bool enable) {
  if (enable) {
    if (!bar_.window.get_style_context()->has_class(classname)) {
      bar_.window.get_style_context()->add_class(classname);
    }
  } else {
    bar_.window.get_style_context()->remove_class(classname);
  }
}

}  // namespace waybar::modules::hyprland
