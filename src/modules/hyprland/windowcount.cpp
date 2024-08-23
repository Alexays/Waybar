#include "modules/hyprland/windowcount.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <vector>

#include "modules/hyprland/backend.hpp"
#include "util/rewrite_string.hpp"
#include "util/sanitize_str.hpp"

namespace waybar::modules::hyprland {

WindowCount::WindowCount(const std::string& id, const Bar& bar, const Json::Value& config)
    : AAppIconLabel(config, "window", id, "{title}", 0, true), bar_(bar) {
  modulesReady = true;
  separateOutputs_ = config["separate-outputs"].asBool();

  if (!gIPC) {
    gIPC = std::make_unique<IPC>();
  }

  queryActiveWorkspace();
  update();
  dp.emit();

  // register for hyprland ipc
  gIPC->registerForIPC("activewindow", this);
  gIPC->registerForIPC("closewindow", this);
  gIPC->registerForIPC("movewindow", this);
  gIPC->registerForIPC("changefloatingmode", this);
  gIPC->registerForIPC("fullscreen", this);
}

WindowCount::~WindowCount() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

auto WindowCount::update() -> void {
  // fix ampersands
  std::lock_guard<std::mutex> lg(mutex_);

  std::string windowName = waybar::util::sanitize_string(workspace_.last_window_title);
  std::string windowAddress = workspace_.last_window;

  windowData_.title = windowName;

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(waybar::util::rewriteString(
        fmt::format(fmt::runtime(format_), fmt::arg("title", windowName),
                    fmt::arg("initialTitle", windowData_.initial_title),
                    fmt::arg("class", windowData_.class_name),
                    fmt::arg("initialClass", windowData_.initial_class_name)),
        config_["rewrite"]));
  } else {
    label_.hide();
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
      return Workspace{-1, 0, "", ""};
    }
    const int id = (*monitor)["activeWorkspace"]["id"].asInt();

    const auto workspaces = gIPC->getSocket1JsonReply("workspaces");
    if (workspaces.isArray()) {
      auto workspace = std::find_if(workspaces.begin(), workspaces.end(),
                                    [&](Json::Value workspace) { return workspace["id"] == id; });
      if (workspace == std::end(workspaces)) {
        spdlog::warn("No workspace with id {}", id);
        return Workspace{-1, 0, "", ""};
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
      value["lastwindow"].asString(),
      value["lastwindowtitle"].asString(),
  };
}

auto WindowCount::WindowCountData::parse(const Json::Value& value) -> WindowCount::WindowCountData {
  return WindowCountData{value["floating"].asBool(),   value["monitor"].asInt(),
                    value["class"].asString(),    value["initialClass"].asString(),
                    value["title"].asString(),    value["initialTitle"].asString(),
                    value["fullscreen"].asBool(), !value["grouped"].empty()};
}

void WindowCount::queryActiveWorkspace() {
  std::lock_guard<std::mutex> lg(mutex_);

  if (separateOutputs_) {
    workspace_ = getActiveWorkspace(this->bar_.output->name);
  } else {
    workspace_ = getActiveWorkspace();
  }

  focused_ = true;
  if (workspace_.windows > 0) {
    const auto clients = gIPC->getSocket1JsonReply("clients");
    if (clients.isArray()) {
      auto activeWindowCount = std::find_if(clients.begin(), clients.end(), [&](Json::Value window) {
        return window["address"] == workspace_.last_window;
      });

      if (activeWindowCount == std::end(clients)) {
        focused_ = false;
        return;
      }

      windowData_ = WindowCountData::parse(*activeWindowCount);
      updateAppIconName(windowData_.class_name, windowData_.initial_class_name);
      std::vector<Json::Value> workspaceWindowCounts;
      std::copy_if(clients.begin(), clients.end(), std::back_inserter(workspaceWindowCounts),
                   [&](Json::Value window) {
                     return window["workspace"]["id"] == workspace_.id && window["mapped"].asBool();
                   });
      swallowing_ =
          std::any_of(workspaceWindowCounts.begin(), workspaceWindowCounts.end(), [&](Json::Value window) {
            return !window["swallowing"].isNull() && window["swallowing"].asString() != "0x0";
          });
      std::vector<Json::Value> visibleWindowCounts;
      std::copy_if(workspaceWindowCounts.begin(), workspaceWindowCounts.end(),
                   std::back_inserter(visibleWindowCounts),
                   [&](Json::Value window) { return !window["hidden"].asBool(); });
      solo_ = 1 == std::count_if(visibleWindowCounts.begin(), visibleWindowCounts.end(),
                                 [&](Json::Value window) { return !window["floating"].asBool(); });
      allFloating_ = std::all_of(visibleWindowCounts.begin(), visibleWindowCounts.end(),
                                 [&](Json::Value window) { return window["floating"].asBool(); });
      fullscreen_ = windowData_.fullscreen;

      // Fullscreen windows look like they are solo
      if (fullscreen_) {
        solo_ = true;
      }

      // Grouped windows have a tab bar and therefore don't look fullscreen or solo
      if (windowData_.grouped) {
        fullscreen_ = false;
        solo_ = false;
      }

      if (solo_) {
        soloClass_ = windowData_.class_name;
      } else {
        soloClass_ = "";
      }
    };
  } else {
    focused_ = false;
    windowData_ = WindowCountData{};
    allFloating_ = false;
    swallowing_ = false;
    fullscreen_ = false;
    solo_ = false;
    soloClass_ = "";
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
