#pragma once

#include <fmt/format.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include <map>
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
            zwlr_workspace_handle_v1 *workspace, uint32_t id);
  ~Workspace();
  auto update() -> void;

  auto id() const -> uint32_t { return id_; }
  auto is_active() const -> bool { return state_ & static_cast<uint32_t>(State::ACTIVE); }
  // wlr stuff
  auto handle_name(const std::string &name) -> void;
  auto handle_coordinates(const std::vector<uint32_t> &coordinates) -> void;
  auto handle_state(const std::vector<uint32_t> &state) -> void;
  auto handle_remove() -> void;

  auto handle_done() -> void;
  auto handle_clicked() -> void;
  auto show() -> void { button_.show(); }
  auto hide() -> void { button_.hide(); }
  auto get_button_ref() -> Gtk::Button & { return button_; }
  auto get_name() -> std::string & { return name_; }
  auto get_coords() -> std::vector<uint32_t> & { return coordinates_; }

  enum class State { ACTIVE = 1 << 0 };

 private:
  auto get_icon() -> std::string;

  const Bar &        bar_;
  const Json::Value &config_;
  WorkspaceGroup &   workspace_group_;

  // wlr stuff
  zwlr_workspace_handle_v1 *workspace_handle_;
  uint32_t                  state_ = 0;

  uint32_t                                  id_;
  std::string                               name_;
  std::vector<uint32_t>                     coordinates_;
  static std::map<std::string, std::string> icons_map_;
  std::string                               format_;
  bool                                      with_icon_ = false;

  Gtk::Button button_;
  Gtk::Box    content_;
  Gtk::Label  label_;
};

class WorkspaceGroup {
 public:
  WorkspaceGroup(const waybar::Bar &bar, Gtk::Box &box, const Json::Value &config,
                 WorkspaceManager &manager, zwlr_workspace_group_handle_v1 *workspace_group_handle,
                 uint32_t id);
  ~WorkspaceGroup();
  auto update() -> void;

  auto id() const -> uint32_t { return id_; }
  auto is_visible() const -> bool { return output_ != nullptr; }
  auto remove_workspace(uint32_t id_) -> void;

  // wlr stuff
  auto handle_workspace_create(zwlr_workspace_handle_v1 *workspace_handle) -> void;
  auto handle_remove() -> void;
  auto handle_output_enter(wl_output *output) -> void;
  auto handle_output_leave() -> void;

  auto add_button(Gtk::Button &button) -> void;
  auto handle_done() -> void;
  auto commit() -> void;
  auto sort_workspaces() -> void;

 private:
  static uint32_t    workspace_global_id;
  const waybar::Bar &bar_;
  Gtk::Box &         box_;
  const Json::Value &config_;
  WorkspaceManager & workspace_manager_;

  // wlr stuff
  zwlr_workspace_group_handle_v1 *workspace_group_handle_;
  wl_output *                     output_ = nullptr;

  uint32_t                                id_;
  std::vector<std::unique_ptr<Workspace>> workspaces_;
  bool                                    sort_by_name = true;
  bool                                    sort_by_coordinates = true;
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

  auto commit() -> void;

 private:
  const waybar::Bar &                          bar_;
  Gtk::Box                                     box_;
  std::vector<std::unique_ptr<WorkspaceGroup>> groups_;

  // wlr stuff
  zwlr_workspace_manager_v1 *workspace_manager_ = nullptr;

  static uint32_t group_global_id;
};

}  // namespace waybar::modules::wlr
