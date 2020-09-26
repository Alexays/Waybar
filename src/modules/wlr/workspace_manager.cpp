#include "modules/wlr/workspace_manager.hpp"

#include <gdk/gdkwayland.h>
#include <gtkmm.h>
#include <spdlog/spdlog.h>

#include <algorithm>

#include "modules/wlr/workspace_manager_binding.hpp"

namespace waybar::modules::wlr {

uint32_t                           WorkspaceGroup::workspace_global_id = 0;
uint32_t                           WorkspaceManager::group_global_id = 0;
std::map<std::string, std::string> Workspace::icons_map_;

WorkspaceManager::WorkspaceManager(const std::string &id, const waybar::Bar &bar,
                                   const Json::Value &config)
    : waybar::AModule(config, "workspaces", id, false, false),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);

  add_registry_listener(this);
  if (!workspace_manager_) {
    return;
  }
}

auto WorkspaceManager::register_manager(wl_registry *registry, uint32_t name, uint32_t version)
    -> void {
  if (workspace_manager_) {
    spdlog::warn("Register workspace manager again although already registered!");
    return;
  }
  if (version != 1) {
    spdlog::warn("Using different workspace manager protocol version: {}", version);
  }
  workspace_manager_ = workspace_manager_bind(registry, name, version, this);
}

auto WorkspaceManager::handle_workspace_group_create(
    zwlr_workspace_group_handle_v1 *workspace_group_handle) -> void {
  auto new_id = ++group_global_id;
  groups_.push_back(
      std::make_unique<WorkspaceGroup>(bar_, box_, config_, *this, workspace_group_handle, new_id));
  spdlog::debug("Workspace group {} created", new_id);
}

auto WorkspaceManager::handle_finished() -> void {
  zwlr_workspace_manager_v1_destroy(workspace_manager_);
  workspace_manager_ = nullptr;
}

auto WorkspaceManager::handle_done() -> void {
  for (auto &group : groups_) {
    group->handle_done();
  }
  dp.emit();
}

auto WorkspaceManager::update() -> void {
  for (auto &group : groups_) {
    group->update();
  }
  AModule::update();
}

WorkspaceManager::~WorkspaceManager() {
  if (!workspace_manager_) {
    return;
  }

  zwlr_workspace_manager_v1_destroy(workspace_manager_);
  workspace_manager_ = nullptr;
}

auto WorkspaceManager::remove_workspace_group(uint32_t id) -> void {
  auto it = std::find_if(groups_.begin(),
                         groups_.end(),
                         [id](const std::unique_ptr<WorkspaceGroup> &g) { return g->id() == id; });

  if (it == groups_.end()) {
    spdlog::warn("Can't find group with id {}", id);
    return;
  }

  groups_.erase(it);
}
auto WorkspaceManager::commit() -> void { zwlr_workspace_manager_v1_commit(workspace_manager_); }

WorkspaceGroup::WorkspaceGroup(const Bar &bar, Gtk::Box &box, const Json::Value &config,
                               WorkspaceManager &              manager,
                               zwlr_workspace_group_handle_v1 *workspace_group_handle, uint32_t id)
    : bar_(bar),
      box_(box),
      config_(config),
      workspace_manager_(manager),
      workspace_group_handle_(workspace_group_handle),
      id_(id) {
  add_workspace_group_listener(workspace_group_handle, this);
  auto config_sort_by_name = config_["sort-by-name"];
  if (config_sort_by_name.isBool()) {
    sort_by_name = config_sort_by_name.asBool();
  }

  auto config_sort_by_coordinates = config_["sort-by-coordinates"];
  if (config_sort_by_coordinates.isBool()) {
    sort_by_coordinates = config_sort_by_coordinates.asBool();
  }
}

auto WorkspaceGroup::add_button(Gtk::Button &button) -> void {
  box_.pack_start(button, false, false);
}

WorkspaceGroup::~WorkspaceGroup() {
  if (!workspace_group_handle_) {
    return;
  }

  zwlr_workspace_group_handle_v1_destroy(workspace_group_handle_);
  workspace_group_handle_ = nullptr;
}

auto WorkspaceGroup::handle_workspace_create(zwlr_workspace_handle_v1 *workspace) -> void {
  auto new_id = ++workspace_global_id;
  workspaces_.push_back(std::make_unique<Workspace>(bar_, config_, *this, workspace, new_id));
  spdlog::debug("Workspace {} created", new_id);
}

auto WorkspaceGroup::handle_remove() -> void {
  zwlr_workspace_group_handle_v1_destroy(workspace_group_handle_);
  workspace_group_handle_ = nullptr;
  workspace_manager_.remove_workspace_group(id_);
}

auto WorkspaceGroup::handle_output_enter(wl_output *output) -> void {
  spdlog::debug("Output {} assigned to {} group", (void *)output, id_);
  output_ = output;

  if (output != gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj())) {
    return;
  }

  for (auto &workspace : workspaces_) {
    workspace->show();
  }
}

auto WorkspaceGroup::handle_output_leave() -> void {
  spdlog::debug("Output {} remove from {} group", (void *)output_, id_);
  output_ = nullptr;

  if (output_ != gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj())) {
    return;
  }

  for (auto &workspace : workspaces_) {
    workspace->hide();
  }
}

auto WorkspaceGroup::update() -> void {
  for (auto &workspace : workspaces_) {
    workspace->update();
  }
}

auto WorkspaceGroup::remove_workspace(uint32_t id) -> void {
  auto it = std::find_if(workspaces_.begin(),
                         workspaces_.end(),
                         [id](const std::unique_ptr<Workspace> &w) { return w->id() == id; });

  if (it == workspaces_.end()) {
    spdlog::warn("Can't find workspace with id {}", id);
    return;
  }

  workspaces_.erase(it);
}

auto WorkspaceGroup::handle_done() -> void {
  for (auto &workspace : workspaces_) {
    workspace->handle_done();
  }
}

auto WorkspaceGroup::commit() -> void { workspace_manager_.commit(); }

auto WorkspaceGroup::sort_workspaces() -> void {
  auto cmp = [=](std::unique_ptr<Workspace> &lhs, std::unique_ptr<Workspace> &rhs) {
    auto is_name_less = lhs->get_name() < rhs->get_name();
    auto is_name_eq = lhs->get_name() == rhs->get_name();
    auto is_coords_less = lhs->get_coords() < rhs->get_coords();
    if (sort_by_name) {
      if (sort_by_coordinates) {
        return is_name_eq ? is_coords_less : is_name_less;
      }
      else {
        return is_name_less;
      }
    }

    if (sort_by_coordinates) {
      return is_coords_less;
    }

    return lhs->id() < rhs->id();
  };

  std::sort(workspaces_.begin(), workspaces_.end(), cmp);
  for (size_t i = 0; i < workspaces_.size(); ++i) {
    for (auto &workspace : workspaces_) {
      box_.reorder_child(workspace->get_button_ref(), i);
    }
  }
}

auto WorkspaceGroup::remove_button(Gtk::Button &button) -> void {
  box_.remove(button);
}

Workspace::Workspace(const Bar &bar, const Json::Value &config, WorkspaceGroup &workspace_group,
                     zwlr_workspace_handle_v1 *workspace, uint32_t id)
    : bar_(bar),
      config_(config),
      workspace_group_(workspace_group),
      workspace_handle_(workspace),
      id_(id) {
  add_workspace_listener(workspace, this);

  auto config_format = config["format"];

  format_ = config_format.isString() ? config_format.asString() : "{name}";
  with_icon_ = format_.find("{icon}") != std::string::npos;

  if (with_icon_ && icons_map_.empty()) {
    auto format_icons = config["format-icons"];
    for (auto &name : format_icons.getMemberNames()) {
      icons_map_.emplace(name, format_icons[name].asString());
    }
  }

  button_.signal_clicked().connect(sigc::mem_fun(this, &Workspace::handle_clicked));

  workspace_group.add_button(button_);
  button_.set_relief(Gtk::RELIEF_NONE);
  content_.set_center_widget(label_);
  button_.add(content_);
  if (!workspace_group.is_visible()) {
    return;
  }

  button_.show();
  label_.show();
  content_.show();
}

Workspace::~Workspace() {
  workspace_group_.remove_button(button_);
  if (!workspace_handle_) {
    return;
  }

  zwlr_workspace_handle_v1_destroy(workspace_handle_);
  workspace_handle_ = nullptr;
}

auto Workspace::update() -> void {
  label_.set_markup(fmt::format(
      format_, fmt::arg("name", name_), fmt::arg("icon", with_icon_ ? get_icon() : "")));
}

auto Workspace::handle_state(const std::vector<uint32_t> &state) -> void {
  state_ = 0;
  for (auto state_entry : state) {
    switch (state_entry) {
      case ZWLR_WORKSPACE_HANDLE_V1_STATE_ACTIVE:
        state_ |= (uint32_t)State::ACTIVE;
        break;
    }
  }
}

auto Workspace::handle_remove() -> void {
  zwlr_workspace_handle_v1_destroy(workspace_handle_);
  workspace_handle_ = nullptr;
  workspace_group_.remove_workspace(id_);
}

auto Workspace::handle_done() -> void {
  spdlog::debug("Workspace {} changed to state {}", id_, state_);
  auto style_context = button_.get_style_context();
  if (is_active()) {
    style_context->add_class("focused");
  } else {
    style_context->remove_class("focused");
  }
}

auto Workspace::get_icon() -> std::string {
  if (is_active()) {
    auto focused_icon_it = icons_map_.find("focused");
    if (focused_icon_it != icons_map_.end()) {
      return focused_icon_it->second;
    }
  }

  auto named_icon_it = icons_map_.find(name_);
  if (named_icon_it != icons_map_.end()) {
    return named_icon_it->second;
  }

  auto default_icon_it = icons_map_.find("default");
  if (default_icon_it != icons_map_.end()) {
    return default_icon_it->second;
  }

  return name_;
}

auto Workspace::handle_clicked() -> void {
  spdlog::debug("Workspace {} clicked", (void *)workspace_handle_);
  zwlr_workspace_handle_v1_activate(workspace_handle_);
  workspace_group_.commit();
}

auto Workspace::handle_name(const std::string &name) -> void {
  name_ = name;
  workspace_group_.sort_workspaces();
}

auto Workspace::handle_coordinates(const std::vector<uint32_t> &coordinates) -> void {
  coordinates_ = coordinates;
  workspace_group_.sort_workspaces();
}
}  // namespace waybar::modules::wlr