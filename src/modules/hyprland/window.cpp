#include "modules/hyprland/window.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <regex>
#include <util/sanitize_str.hpp>
#include <vector>

#include "modules/hyprland/backend.hpp"
#include "util/rewrite_string.hpp"

namespace waybar::modules::hyprland {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AAppIconLabel(config, "window", id, "{title}", 0, true), bar_(bar) {
  modulesReady = true;
  separate_outputs = config["separate-outputs"].asBool();

  if (!gIPC.get()) {
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

Window::~Window() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

auto Window::update() -> void {
  // fix ampersands
  std::lock_guard<std::mutex> lg(mutex_);

  std::string window_name = waybar::util::sanitize_string(workspace_.last_window_title);
  std::string window_address = workspace_.last_window;

  window_data_.title = window_name;

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(waybar::util::rewriteString(
        fmt::format(fmt::runtime(format_), fmt::arg("title", window_name),
                    fmt::arg("initialTitle", window_data_.initial_title),
                    fmt::arg("class", window_data_.class_name),
                    fmt::arg("initialClass", window_data_.initial_class_name)),
        config_["rewrite"]));
  } else {
    label_.hide();
  }

  setClass("empty", workspace_.windows == 0);
  setClass("solo", solo_);
  setClass("floating", all_floating_);
  setClass("swallowing", swallowing_);
  setClass("fullscreen", fullscreen_);

  if (!last_solo_class_.empty() && solo_class_ != last_solo_class_) {
    if (bar_.window.get_style_context()->has_class(last_solo_class_)) {
      bar_.window.get_style_context()->remove_class(last_solo_class_);
      spdlog::trace("Removing solo class: {}", last_solo_class_);
    }
  }

  if (!solo_class_.empty() && solo_class_ != last_solo_class_) {
    bar_.window.get_style_context()->add_class(solo_class_);
    spdlog::trace("Adding solo class: {}", solo_class_);
  }
  last_solo_class_ = solo_class_;

  AAppIconLabel::update();
}

auto Window::getActiveWorkspace() -> Workspace {
  const auto workspace = gIPC->getSocket1JsonReply("activeworkspace");
  assert(workspace.isObject());
  return Workspace::parse(workspace);
}

auto Window::getActiveWorkspace(const std::string& monitorName) -> Workspace {
  const auto monitors = gIPC->getSocket1JsonReply("monitors");
  assert(monitors.isArray());
  auto monitor = std::find_if(monitors.begin(), monitors.end(),
                              [&](Json::Value monitor) { return monitor["name"] == monitorName; });
  if (monitor == std::end(monitors)) {
    spdlog::warn("Monitor not found: {}", monitorName);
    return Workspace{-1, 0, "", ""};
  }
  const int id = (*monitor)["activeWorkspace"]["id"].asInt();

  const auto workspaces = gIPC->getSocket1JsonReply("workspaces");
  assert(workspaces.isArray());
  auto workspace = std::find_if(workspaces.begin(), workspaces.end(),
                                [&](Json::Value workspace) { return workspace["id"] == id; });
  if (workspace == std::end(workspaces)) {
    spdlog::warn("No workspace with id {}", id);
    return Workspace{-1, 0, "", ""};
  }
  return Workspace::parse(*workspace);
}

auto Window::Workspace::parse(const Json::Value& value) -> Window::Workspace {
  return Workspace{value["id"].asInt(), value["windows"].asInt(), value["lastwindow"].asString(),
                   value["lastwindowtitle"].asString()};
}

auto Window::WindowData::parse(const Json::Value& value) -> Window::WindowData {
  return WindowData{value["floating"].asBool(),   value["monitor"].asInt(),
                    value["class"].asString(),    value["initialClass"].asString(),
                    value["title"].asString(),    value["initialTitle"].asString(),
                    value["fullscreen"].asBool(), !value["grouped"].empty()};
}

void Window::queryActiveWorkspace() {
  std::lock_guard<std::mutex> lg(mutex_);

  if (separate_outputs) {
    workspace_ = getActiveWorkspace(this->bar_.output->name);
  } else {
    workspace_ = getActiveWorkspace();
  }

  if (workspace_.windows > 0) {
    const auto clients = gIPC->getSocket1JsonReply("clients");
    assert(clients.isArray());
    auto active_window = std::find_if(clients.begin(), clients.end(), [&](Json::Value window) {
      return window["address"] == workspace_.last_window;
    });
    if (active_window == std::end(clients)) {
      return;
    }

    window_data_ = WindowData::parse(*active_window);
    updateAppIconName(window_data_.class_name, window_data_.initial_class_name);
    std::vector<Json::Value> workspace_windows;
    std::copy_if(clients.begin(), clients.end(), std::back_inserter(workspace_windows),
                 [&](Json::Value window) {
                   return window["workspace"]["id"] == workspace_.id && window["mapped"].asBool();
                 });
    swallowing_ = std::any_of(workspace_windows.begin(), workspace_windows.end(),
                              [&](Json::Value window) { return !window["swallowing"].isNull(); });
    std::vector<Json::Value> visible_windows;
    std::copy_if(workspace_windows.begin(), workspace_windows.end(),
                 std::back_inserter(visible_windows),
                 [&](Json::Value window) { return !window["hidden"].asBool(); });
    solo_ = 1 == std::count_if(visible_windows.begin(), visible_windows.end(),
                               [&](Json::Value window) { return !window["floating"].asBool(); });
    all_floating_ = std::all_of(visible_windows.begin(), visible_windows.end(),
                                [&](Json::Value window) { return window["floating"].asBool(); });
    fullscreen_ = window_data_.fullscreen;

    // Fullscreen windows look like they are solo
    if (fullscreen_) {
      solo_ = true;
    }

    // Grouped windows have a tab bar and therefore don't look fullscreen or solo
    if (window_data_.grouped) {
      fullscreen_ = false;
      solo_ = false;
    }

    if (solo_) {
      solo_class_ = window_data_.class_name;
    } else {
      solo_class_ = "";
    }
  } else {
    window_data_ = WindowData{};
    all_floating_ = false;
    swallowing_ = false;
    fullscreen_ = false;
    solo_ = false;
    solo_class_ = "";
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
