#pragma once

#include <fmt/format.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

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
            zwlr_workspace_handle_v1 *workspace);
  ~Workspace();
  auto update() -> void;

  auto id() const -> uint32_t { return id_; }
  // wlr stuff
  auto handle_name(const std::string &name) -> void { name_ = name; }
  auto handle_coordinates(const std::vector<uint32_t> &coordinates) -> void {
    coordinates_ = coordinates;
  }
  auto handle_state(const std::vector<uint32_t> &state) -> void;
  auto handle_remove() -> void;

  enum class State { ACTIVE = 1 << 0 };

 private:
  static uint32_t    workspace_global_id;
  const Bar &        bar_;
  const Json::Value &config_;
  WorkspaceGroup &   workspace_group_;

  // wlr stuff
  zwlr_workspace_handle_v1 *workspace_handle_;
  uint32_t                  state_ = 0;

  uint32_t              id_;
  std::string           name_;
  std::vector<uint32_t> coordinates_;

  Gtk::Button button_;
  Gtk::Box    content_;
  Gtk::Label  label_;
};

class WorkspaceGroup {
 public:
  WorkspaceGroup(const waybar::Bar &bar, const Json::Value &config, WorkspaceManager &manager,
                 zwlr_workspace_group_handle_v1 *workspace_group_handle);
  ~WorkspaceGroup();
  auto update() -> void;

  auto id() const -> uint32_t { return id_; }
  auto remove_workspace(uint32_t id_) -> void;

  // wlr stuff
  auto handle_workspace_create(zwlr_workspace_handle_v1 *workspace_handle) -> void;
  auto handle_remove() -> void;
  auto handle_output_enter(wl_output *output) -> void;
  auto handle_output_leave() -> void;

  auto add_button(Gtk::Button &button) -> void;

 private:
  static uint32_t    group_global_id;
  const waybar::Bar &bar_;
  const Json::Value &config_;
  WorkspaceManager & workspace_manager_;

  // wlr stuff
  zwlr_workspace_group_handle_v1 *workspace_group_handle_;
  wl_output *                     output_ = nullptr;

  uint32_t                                id_;
  std::vector<std::unique_ptr<Workspace>> workspaces_;
};

class WorkspaceManager : public AModule {
 public:
  WorkspaceManager(const std::string &id, const waybar::Bar &bar, const Json::Value &config);
  ~WorkspaceManager() override;
  auto update() -> void override;

  auto remove_workspace_group(uint32_t id_) -> void;

  // wlr stuff
  auto register_manager(wl_registry *registry, uint32_t name, uint32_t version) -> void;
  auto handle_workspace_group_create(zwlr_workspace_group_handle_v1 *workspace_group_handle)
      -> void;
  auto handle_done() -> void;
  auto handle_finished() -> void;

  auto add_button(Gtk::Button &button) -> void { box_.pack_start(button, false, false); }

 private:
  const waybar::Bar &                          bar_;
  Gtk::Box                                     box_;
  std::vector<std::unique_ptr<WorkspaceGroup>> groups_;

  // wlr stuff
  zwlr_workspace_manager_v1 *workspace_manager_ = nullptr;
};

}  // namespace waybar::modules::wlr
