#include "modules/hyprland/workspaces.hpp"

#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <ranges>
#include <vector>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

namespace waybar::modules::hyprland {

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "workspaces", id, false, !config["disable-scroll"].asBool()),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }

  event_box_.add(box_);

  if (config["enable-bar-scroll"].asBool()) {
    auto &window = const_cast<Bar &>(bar_).window;
    window.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    window.signal_scroll_event().connect(sigc::mem_fun(*this, &Workspaces::handleScroll));
  }

  modulesReady = true;

  if (!gIPC.get()) {
    gIPC = std::make_unique<IPC>();
  }

  // register for hyprland ipc
  gIPC->registerForIPC("createworkspace", [&](const std::string &ev) { this->onEvent(ev); });
  gIPC->registerForIPC("destroyworkspace", [&](const std::string &ev) { this->onEvent(ev); });
  gIPC->registerForIPC("activemon", [&](const std::string &ev) { this->onEvent(ev); });
  gIPC->registerForIPC("workspace", [&](const std::string &ev) { this->onEvent(ev); });

  // parse cfg stuff
  configOnLaunch(config);

  // parse workspaces already existing
  parseInitHyprlandWorkspaces();
}

void Workspaces::parseInitHyprlandWorkspaces() {
    std::istringstream WORKSPACES;
    WORKSPACES.str(gIPC->getSocket1Reply("workspaces"));

    std::string line;
    while (std::getline(WORKSPACES, line)) {
        if (line.find("workspace ID") == 0) {
            auto workspaceName = line.substr(line.find_first_of('(') + 1).substr(0, line.find_first_of(')') - line.find_first_of('(') - 1);
            workspaces.emplace_back(workspaceName);
        }
    }
}

void Workspaces::configOnLaunch(const Json::Value& cfg) {
  if (cfg["persistent_workspaces"].isObject()) {
    spdlog::info("persistent");
    const Json::Value &persistentWorkspacesJSON = cfg["persistent_workspaces"];

    for (auto &wn : persistentWorkspacesJSON.getMemberNames()) {
      persistentWorkspaces.emplace_back(wn);
      spdlog::info("persistent ws {}", wn);
    }
  }
}

std::deque<std::string> Workspaces::getAllSortedWS() {
    std::deque<std::string> result;
    for (auto& ws : workspaces)
        result.emplace_back(ws);
    for (auto& ws : persistentWorkspaces)
        result.emplace_back(ws);

    std::sort(result.begin(), result.end(), [&](const std::string& ws1, const std::string& ws2) {
        if (isNumber(ws1) && isNumber(ws2)) {
            return std::stoi(ws1) < std::stoi(ws2);
        } else if (isNumber(ws1)) {
            return true;
        } else {
            return false;
        }
    });

    return result;
}

std::string Workspaces::getIcon(const std::string &name) {
  return config_["format-icons"][name].asString();
}

auto Workspaces::update() -> void {
    updateButtons();
}

void Workspaces::updateButtons() {
  mutex_.lock();

  auto ws = getAllSortedWS();

  for (auto it = ws.begin(); it != ws.end(); ++it) {
    auto bit = buttons_.find(*it);

    auto &button = bit == buttons_.end() ? addButton(*it) : bit->second;

    if (focusedWorkspace == *it) {
      button.get_style_context()->add_class("focused");
    } else {
      button.get_style_context()->remove_class("focused");
    }

    std::string label = *it;
    if (config_["format"].isString()) {
      auto format = config_["format"].asString();
      label = fmt::format(format, fmt::arg("icon", getIcon(*it)), fmt::arg("name", *it));
    }

    button.set_label(label);

    button.show();
  }

  AModule::update();

  mutex_.unlock();
}

bool Workspaces::isNumber(const std::string& str) {
  for (auto &c : str) {
    if (!(isdigit(c) != 0 || c == '-')) return false;
  }
  return true;
}

Gtk::Button &Workspaces::addButton(const std::string& name) {
  auto pair = buttons_.emplace(name, name);
  auto &&button = pair.first->second;
  box_.pack_start(button, false, false, 0);
  button.set_name(name);
  button.set_relief(Gtk::RELIEF_NONE);
  if (!config_["disable-click"].asBool()) {
    button.signal_pressed().connect([&, name]{
      if (isNumber(name)) {
        gIPC->getSocket1Reply("dispatch workspace " + name);
        spdlog::info("executing {}", "dispatch workspace " + name);
      }
      else {
        gIPC->getSocket1Reply("dispatch workspace name:" + name);
        spdlog::info("executing {}", "dispatch workspace name:" + name);
      }
    });
  }

  return button;
}

void Workspaces::onEvent(const std::string& ev) {
    const auto EVENT = ev.substr(0, ev.find_first_of('>'));
    const auto WORKSPACE = ev.substr(ev.find_last_of('>') + 1);

    mutex_.lock();

    if (EVENT == "activemon") {
        focusedWorkspace = WORKSPACE.substr(WORKSPACE.find_first_of(',') + 1);
    } else if (EVENT == "workspace") {
        focusedWorkspace = WORKSPACE;
    } else if (EVENT == "createworkspace") {
        workspaces.emplace_back(WORKSPACE);

        // remove the buttons for reorder
        buttons_.clear();
    } else {
        const auto it = std::remove(workspaces.begin(), workspaces.end(), WORKSPACE);

        if (it != workspaces.end())
            workspaces.erase(it);

        // also remove the buttons
        buttons_.clear();
    }

    dp.emit();

    mutex_.unlock();
}

bool Workspaces::handleScroll(GdkEventScroll *e) {
  if (gdk_event_get_pointer_emulated((GdkEvent *)e)) {
    /**
     * Ignore emulated scroll events on window
     */
    return false;
  }
  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }

  mutex_.lock();

  if (dir == SCROLL_DIR::UP) {
    gIPC->getSocket1Reply("dispatch workspace +1");
  } else if (dir == SCROLL_DIR::DOWN) {
    gIPC->getSocket1Reply("dispatch workspace -1");
  }

  mutex_.unlock();

  return true;
}
};
