#include "modules/wlr/workspace_manager.hpp"

#include <gtkmm.h>
#include <spdlog/spdlog.h>

#include <algorithm>

#include "modules/wlr/workspace_manager_binding.hpp"

namespace waybar::modules::wlr {

uint32_t Workspace::workspace_global_id = 0;
uint32_t WorkspaceGroup::group_global_id = 0;

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
  groups_.push_back(std::make_unique<WorkspaceGroup>(bar_, config_, *this, workspace_group_handle));
  spdlog::debug("Workspace group {} created", groups_.back()->id());
}

auto WorkspaceManager::handle_finished() -> void {
  zwlr_workspace_manager_v1_destroy(workspace_manager_);
  workspace_manager_ = nullptr;
}

auto WorkspaceManager::handle_done() -> void {
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

WorkspaceGroup::WorkspaceGroup(const Bar &bar, const Json::Value &config, WorkspaceManager &manager,
                               zwlr_workspace_group_handle_v1 *workspace_group_handle)
    : bar_(bar),
      config_(config),
      workspace_manager_(manager),
      workspace_group_handle_(workspace_group_handle),
      id_(++group_global_id) {
  add_workspace_group_listener(workspace_group_handle, this);
}
auto WorkspaceGroup::add_button(Gtk::Button &button) -> void {
  workspace_manager_.add_button(button);
}

WorkspaceGroup::~WorkspaceGroup() {
  if (!workspace_group_handle_) {
    return;
  }

  zwlr_workspace_group_handle_v1_destroy(workspace_group_handle_);
  workspace_group_handle_ = nullptr;
}

auto WorkspaceGroup::handle_workspace_create(zwlr_workspace_handle_v1 *workspace) -> void {
  workspaces_.push_back(std::make_unique<Workspace>(bar_, config_, *this, workspace));
  spdlog::debug("Workspace {} created", workspaces_.back()->id());
}

auto WorkspaceGroup::handle_remove() -> void {
  zwlr_workspace_group_handle_v1_destroy(workspace_group_handle_);
  workspace_group_handle_ = nullptr;
  workspace_manager_.remove_workspace_group(id_);
}

auto WorkspaceGroup::handle_output_enter(wl_output *output) -> void {
  spdlog::debug("Output {} assigned to {} group", (void *)output, id_);
  output_ = output;
}

auto WorkspaceGroup::handle_output_leave() -> void {
  spdlog::debug("Output {} remove from {} group", (void *)output_, id_);
  output_ = nullptr;
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

Workspace::Workspace(const Bar &bar, const Json::Value &config, WorkspaceGroup &workspace_group,
                     zwlr_workspace_handle_v1 *workspace)
    : bar_(bar),
      config_(config),
      workspace_group_(workspace_group),
      workspace_handle_(workspace),
      id_(++workspace_global_id),
      content_{bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0} {
  add_workspace_listener(workspace, this);
  workspace_group.add_button(button_);
  button_.set_relief(Gtk::RELIEF_NONE);
  label_.set_label(fmt::format("{name}", fmt::arg("name", "1")));
  label_.show();
  content_.add(label_);
  content_.show();
  button_.add(content_);
  button_.show();
}

Workspace::~Workspace() {
  if (!workspace_handle_) {
    return;
  }

  zwlr_workspace_handle_v1_destroy(workspace_handle_);
  workspace_handle_ = nullptr;
}

auto Workspace::update() -> void {
  label_.set_label(fmt::format("{name}", fmt::arg("name", name_)));
  label_.show();
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
}  // namespace waybar::modules::wlr