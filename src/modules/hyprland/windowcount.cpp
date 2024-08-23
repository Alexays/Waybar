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
    : AAppIconLabel(config, "windowcount", id, "{count}", 0, true), bar_(bar) {
  modulesReady = true;
  separateOutputs_ = config["separate-outputs"].asBool();

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

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(waybar::util::rewriteString(
        fmt::format(fmt::runtime(format_), fmt::arg("count", workspace_.windows)),
        config_["rewrite"]));
  } else {
    label_.hide();
  }

  // Display the count as the label text
  label_.set_text(fmt::format("{}", workspace_.windows));

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

void WindowCount::queryActiveWorkspace() {
  std::lock_guard<std::mutex> lg(mutex_);

  if (separateOutputs_) {
    workspace_ = getActiveWorkspace(this->bar_.output->name);
  } else {
    workspace_ = getActiveWorkspace();
  }

  focused_ = true;
  windowCount_ = workspace_.windows;

  if (workspace_.windows == 0) {
    focused_ = false;
  }
}

void WindowCount::onEvent(const std::string& ev) {
  queryActiveWorkspace();
  update();
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