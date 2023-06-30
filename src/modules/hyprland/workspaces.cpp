#include "modules/hyprland/workspaces.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>
#include <string>

namespace waybar::modules::hyprland {

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "workspaces", id, false, false),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  Json::Value config_format = config["format"];

  format_ = config_format.isString() ? config_format.asString() : "{id}";
  with_icon_ = format_.find("{icon}") != std::string::npos;

  if (with_icon_ && icons_map_.empty()) {
    Json::Value format_icons = config["format-icons"];
    for (std::string &name : format_icons.getMemberNames()) {
      icons_map_.emplace(name, format_icons[name].asString());
    }

    icons_map_.emplace("", "");
  }

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

  gIPC->registerForIPC("workspace", this);
  gIPC->registerForIPC("createworkspace", this);
  gIPC->registerForIPC("destroyworkspace", this);
}

auto Workspaces::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  for (Workspace &workspace : workspaces_) {
    workspace.set_active(workspace.id() == active_workspace_id);

    std::string &workspace_icon = icons_map_[""];
    if (with_icon_) {
      workspace_icon = workspace.select_icon(icons_map_);
    }

    workspace.update(format_, workspace_icon);
  }

  AModule::update();
}

void Workspaces::onEvent(const std::string &ev) {
  std::string eventName(begin(ev), begin(ev) + ev.find_first_of('>'));
  std::string payload = ev.substr(eventName.size() + 2);
  if (eventName == "workspace") {
    std::from_chars(payload.data(), payload.data() + payload.size(), active_workspace_id);
  } else if (eventName == "destroyworkspace") {
    int deleted_workspace_id;
    std::from_chars(payload.data(), payload.data() + payload.size(), deleted_workspace_id);
    workspaces_.erase(std::remove_if(workspaces_.begin(), workspaces_.end(),
                                     [&](Workspace &x) { return x.id() == deleted_workspace_id; }));
  } else if (eventName == "createworkspace") {
    int new_workspace_id;
    std::from_chars(payload.data(), payload.data() + payload.size(), new_workspace_id);
    workspaces_.push_back(new_workspace_id);
    Gtk::Button& new_workspace_button = workspaces_.back().button();
    box_.pack_end(new_workspace_button, false, false);
    sort_workspaces();
    new_workspace_button.show_all();
  }

  dp.emit();
}

void Workspaces::init() {
  const auto activeWorkspace = WorkspaceDto::parse(gIPC->getSocket1JsonReply("activeworkspace"));
  active_workspace_id = activeWorkspace.id;
  const Json::Value workspaces_json = gIPC->getSocket1JsonReply("workspaces");
  for (const Json::Value &workspace_json : workspaces_json) {
    workspaces_.push_back(Workspace(WorkspaceDto::parse(workspace_json)));
  }

  for (auto &workspace : workspaces_) {
    box_.pack_start(workspace.button(), false, false);
  }

  sort_workspaces();

  dp.emit();
}

Workspaces::~Workspaces() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

WorkspaceDto WorkspaceDto::parse(const Json::Value &value) {
  return WorkspaceDto{value["id"].asInt()};
}

Workspace::Workspace(WorkspaceDto dto) : Workspace(dto.id){};

Workspace::Workspace(int id) : id_(id) {
  button_.set_relief(Gtk::RELIEF_NONE);
  content_.set_center_widget(label_);
  button_.add(content_);
};

void add_or_remove_class(Glib::RefPtr<Gtk::StyleContext> context, bool condition,
                         const std::string &class_name) {
  if (condition) {
    context->add_class(class_name);
  } else {
    context->remove_class(class_name);
  }
}

void Workspace::update(const std::string &format, const std::string &icon) {
  Glib::RefPtr<Gtk::StyleContext> style_context = button_.get_style_context();
  add_or_remove_class(style_context, active(), "active");

  label_.set_markup(
      fmt::format(fmt::runtime(format), fmt::arg("id", id()), fmt::arg("icon", icon)));
}

void Workspaces::sort_workspaces() {
  std::sort(workspaces_.begin(), workspaces_.end(),
            [](Workspace &lhs, Workspace &rhs) { return lhs.id() < rhs.id(); });

  for (size_t i = 0; i < workspaces_.size(); ++i) {
    box_.reorder_child(workspaces_[i].button(), i);
  }
}

std::string &Workspace::select_icon(std::map<std::string, std::string> &icons_map) {
  if (active()) {
    auto active_icon_it = icons_map.find("active");
    if (active_icon_it != icons_map.end()) {
      return active_icon_it->second;
    }
  }

  auto named_icon_it = icons_map.find(std::to_string(id()));
  if (named_icon_it != icons_map.end()) {
    return named_icon_it->second;
  }

  auto default_icon_it = icons_map.find("default");
  if (default_icon_it != icons_map.end()) {
    return default_icon_it->second;
  }

  return icons_map[""];
}
}  // namespace waybar::modules::hyprland
