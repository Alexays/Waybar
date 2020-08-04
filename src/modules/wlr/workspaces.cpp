#include "modules/wlr/workspaces.hpp"

#include <gtkmm.h>
#include <spdlog/spdlog.h>

#include <client.hpp>

namespace waybar::modules::wlr {

static void handle_global(void *data, wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
  if (std::strcmp(interface, zwlr_workspace_manager_v1_interface.name) == 0) {
    static_cast<WorkspaceManager *>(data)->register_manager(registry, name, version);
  }
}

static void handle_global_remove(void *data, wl_registry *registry, uint32_t name) {
  /* Nothing to do here */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

WorkspaceManager::WorkspaceManager(const std::string &id, const waybar::Bar &bar,
                                   const Json::Value &config)
    : waybar::AModule(config, "workspaces", id, false, !config["disable-scroll"].asBool()),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);

  // wlr stuff
  wl_display * display = Client::inst()->wl_display;
  wl_registry *registry = wl_display_get_registry(display);

  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

  if (!workspace_manager_) {
    return;
  }
}

static void workspace_manager_handle_workspace_group(
    void *data, zwlr_workspace_manager_v1 *_,
    zwlr_workspace_group_handle_v1 *workspace_group) {
  static_cast<WorkspaceManager *>(data)->handle_workspace_group_create(workspace_group);
}

static void workspace_manager_handle_done(void *data, zwlr_workspace_manager_v1 *_) {
  static_cast<WorkspaceManager *>(data)->handle_done();
}

static void workspace_manager_handle_finished(void *data, zwlr_workspace_manager_v1 *_) {
  static_cast<WorkspaceManager *>(data)->handle_finished();
}

static const zwlr_workspace_manager_v1_listener workspace_manager_impl = {
    .workspace_group = workspace_manager_handle_workspace_group,
    .done = workspace_manager_handle_done,
    .finished = workspace_manager_handle_finished,
};

auto WorkspaceManager::register_manager(wl_registry *registry, uint32_t name, uint32_t version)
    -> void {
  if (workspace_manager_) {
    spdlog::warn("Register workspace manager again although already registered!");
    return;
  }
  if (version != 1) {
    spdlog::warn("Using different workspace manager protocol version: {}", version);
  }

  workspace_manager_ = static_cast<zwlr_workspace_manager_v1 *>(
      wl_registry_bind(registry, name, &zwlr_workspace_manager_v1_interface, version));

  if (workspace_manager_)
    zwlr_workspace_manager_v1_add_listener(workspace_manager_, &workspace_manager_impl, this);
  else
    spdlog::debug("Failed to register manager");
}
auto WorkspaceManager::handle_workspace_group_create(
    zwlr_workspace_group_handle_v1 *workspace_group_handle) -> void {
  groups_.push_back(std::make_unique<WorkspaceGroup>(bar_, config_, *this, *workspace_group_handle));
}

auto WorkspaceManager::handle_finished() -> void {
  zwlr_workspace_manager_v1_destroy(workspace_manager_);
  workspace_manager_ = nullptr;
}
auto WorkspaceManager::handle_done() -> void {}
auto WorkspaceManager::update() -> void {
  for (auto &group : groups_) {
    group->update();
  }
  AModule::update();
}

static void workspace_group_handle_output_enter(void *data,
                                                zwlr_workspace_group_handle_v1 *_,
                                                wl_output *output) {
  static_cast<WorkspaceGroup*>(data)->handle_output_enter(*output);
}

static void workspace_group_handle_output_leave(void *data,
                                                zwlr_workspace_group_handle_v1 *_,
                                                wl_output *output) {
  static_cast<WorkspaceGroup*>(data)->handle_output_leave(*output);
}

static void workspace_group_handle_workspace(void *data,
                         zwlr_workspace_group_handle_v1 *_,
                         zwlr_workspace_handle_v1 *workspace) {
  static_cast<WorkspaceGroup*>(data)->handle_workspace_create(*workspace);
}

static void workspace_group_handle_remove(void *data,
                                             zwlr_workspace_group_handle_v1 *_) {
  static_cast<WorkspaceGroup*>(data)->handle_remove();
}

static const zwlr_workspace_group_handle_v1_listener workspace_group_impl = {
    .output_enter = workspace_group_handle_output_enter,
    .output_leave = workspace_group_handle_output_leave,
    .workspace = workspace_group_handle_workspace,
    .remove = workspace_group_handle_remove
};

WorkspaceGroup::WorkspaceGroup(const Bar & bar, const Json::Value &config, WorkspaceManager &manager,
                               zwlr_workspace_group_handle_v1 &workspace_group_handle)
  : bar_(bar), config_(config), workspace_manager_(manager), workspace_group_handle_(workspace_group_handle)
{
  zwlr_workspace_group_handle_v1_add_listener(&workspace_group_handle, &workspace_group_impl, this);
}
auto WorkspaceGroup::handle_workspace_create(zwlr_workspace_handle_v1 &workspace) -> void {
  workspaces_.push_back(std::make_unique<Workspace>(bar_, config_, *this, workspace));
}
auto WorkspaceGroup::handle_remove() -> void {}
auto WorkspaceGroup::handle_output_enter(wl_output &output) -> void {}
auto WorkspaceGroup::handle_output_leave(wl_output &output) -> void {}
auto WorkspaceGroup::update() -> void {

}
Workspace::Workspace(const Bar &bar, const Json::Value &config,
                     WorkspaceGroup &workspace_group, zwlr_workspace_handle_v1 &workspace)
  : bar_(bar), config_(config), workspace_group_(workspace_group), workspace_handle_(workspace)
{
}
}  // namespace waybar::modules::wlr