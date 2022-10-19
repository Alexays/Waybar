#include "modules/hyprland/window.hpp"

#include <spdlog/spdlog.h>

#include <regex>
#include <util/sanitize_str.hpp>

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
    label_.set_markup(fmt::format(format_, rewriteTitle(lastView)));
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
  auto monitor = std::find_if(json.begin(), json.end(),
                              [&](Json::Value monitor) { return monitor["name"] == monitorName; });
  assert(monitor != std::end(json));
  return (*monitor)["activeWorkspace"]["id"].as<uint>();
}

std::string Window::getLastWindowTitle(uint workspaceID) {
  auto cmd = waybar::util::command::exec("hyprctl workspaces -j");
  assert(cmd.exit_code == 0);
  Json::Value json = parser_.parse(cmd.out);
  assert(json.isArray());
  auto workspace = std::find_if(json.begin(), json.end(), [&](Json::Value workspace) {
    return workspace["id"].as<uint>() == workspaceID;
  });

  if (workspace != std::end(json)) {
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

std::string Window::rewriteTitle(const std::string& title) {
  const auto& rules = config_["rewrite"];
  if (!rules.isObject()) {
    return title;
  }

  std::string res = title;

  for (auto it = rules.begin(); it != rules.end(); ++it) {
    if (it.key().isString() && it->isString()) {
      try {
        // malformated regexes will cause an exception.
        // in this case, log error and try the next rule.
        const std::regex rule{it.key().asString()};
        if (std::regex_match(title, rule)) {
          res = std::regex_replace(res, rule, it->asString());
        }
      } catch (const std::regex_error& e) {
        spdlog::error("Invalid rule {}: {}", it.key().asString(), e.what());
      }
    }
  }

  return res;
}

}  // namespace waybar::modules::hyprland