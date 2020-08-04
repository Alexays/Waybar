#pragma once

#include <fmt/format.h>
#include <gtkmm/image.h>

#include <memory>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "wlr-workspace-unstable-v1-client-protocol.h"

namespace waybar::modules::wlr {

class WorkspaceManager;
class WorkspaceGroup;

class Workspace {
 public:
  Workspace(const waybar::Bar &bar, const Json::Value &config, WorkspaceGroup &workspace_group,
            zwlr_workspace_handle_v1 &workspace);
  ~Workspace() = default;
  auto update() -> void;

  enum class State { ACTIVE = 1 << 1 };

 private:
  const Bar &           bar_;
  const Gtk::Box        box_;
  const Gtk::Image      icon_;
  const Json::Value &   config_;
  const WorkspaceGroup &workspace_group_;

  // wlr stuff
  zwlr_workspace_handle_v1 &workspace_handle_;
  uint32_t                  state_;
};

class WorkspaceGroup {
 public:
  WorkspaceGroup(const waybar::Bar &bar, const Json::Value &config, WorkspaceManager &manager,
                 zwlr_workspace_group_handle_v1 &workspace_group_handle);
  auto update() -> void;

  // wlr stuff
  auto handle_workspace_create(zwlr_workspace_handle_v1 &workspace_handle) -> void;
  auto handle_remove() -> void;
  auto handle_output_enter(wl_output &output) -> void;
  auto handle_output_leave(wl_output &output) -> void;

 private:
  const waybar::Bar &                     bar_;
  const Json::Value &                     config_;
  const WorkspaceManager &                workspace_manager_;
  std::vector<std::unique_ptr<Workspace>> workspaces_;

  // wlr stuff
  zwlr_workspace_group_handle_v1 &workspace_group_handle_;
};

class WorkspaceManager : public AModule {
 public:
  WorkspaceManager(const std::string &id, const waybar::Bar &bar, const Json::Value &config);
  auto update() -> void override;

  // wlr stuff
  auto register_manager(wl_registry *registry, uint32_t name, uint32_t version) -> void;
  auto handle_workspace_group_create(zwlr_workspace_group_handle_v1 *workspace_group_handle)
      -> void;
  auto handle_done() -> void;
  auto handle_finished() -> void;

 private:
  const waybar::Bar &                          bar_;
  Gtk::Box                               box_;
  std::vector<std::unique_ptr<WorkspaceGroup>> groups_;

  // wlr stuff
  zwlr_workspace_manager_v1 *workspace_manager_ = nullptr;
};

}  // namespace waybar::modules::wlr
