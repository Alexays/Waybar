#include "modules/hyprland/workspaces.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace waybar::modules::hyprland {
Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "workspaces", id, false, false),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);
  modulesReady = true;
  if (!gIPC.get()) {
    gIPC = std::make_unique<IPC>();
  }

  init();

  gIPC->registerForIPC("createworkspace", this);
  gIPC->registerForIPC("destroyworkspace", this);
  gIPC->registerForIPC("urgent", this);
}

auto Workspaces::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  for (Workspace &workspace : workspaces) {
    workspace.update();
  }
  AModule::update();
}

void Workspaces::onEvent(const std::string &ev) { dp.emit(); }

void Workspaces::init() {
  const auto activeWorkspace = Workspace::parse(gIPC->getSocket1JsonReply("activeworkspace"));
  const Json::Value workspaces_json = gIPC->getSocket1JsonReply("workspaces");
  for (const Json::Value &workspace_json : workspaces_json) {
    workspaces.push_back(Workspace::parse(workspace_json));
  }
  std::sort(workspaces.begin(), workspaces.end(),
            [](Workspace &lhs, Workspace &rhs) { return lhs.id() < rhs.id(); });
  for (auto &workspace : workspaces) {
    box_.pack_start(workspace.button(), false, false);
  }

  dp.emit();
}

Workspaces::~Workspaces() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

Workspace Workspace::Workspace::parse(const Json::Value &value) {
  return Workspace{value["id"].asInt()};
}

Workspace::Workspace(int id) : id_(id) {
  button_.set_relief(Gtk::RELIEF_NONE);
  content_.set_center_widget(label_);
  button_.add(content_);
};

void Workspace::update() { label_.set_text(std::to_string(id_)); }
}  // namespace waybar::modules::hyprland
