#pragma once

#include <fmt/format.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "ext-workspace-unstable-v1-client-protocol.h"

namespace waybar::modules::wlr {

class WorkspaceManager;
class WorkspaceGroup;

class Workspace {
 public:
  Workspace(const waybar::Bar &bar, const Json::Value &config, WorkspaceGroup &workspace_group,
            zext_workspace_handle_v1 *workspace, uint32_t id, std::string name);
  ~Workspace();
  auto update() -> void;

  auto id() const -> uint32_t { return id_; }
  auto is_active() const -> bool { return state_ & static_cast<uint32_t>(State::ACTIVE); }
  auto is_urgent() const -> bool { return state_ & static_cast<uint32_t>(State::URGENT); }
  auto is_hidden() const -> bool { return state_ & static_cast<uint32_t>(State::HIDDEN); }
  auto is_empty() const -> bool { return state_ & static_cast<uint32_t>(State::EMPTY); }
  auto is_persistent() const -> bool { return persistent_; }
  // wlr stuff
  auto handle_name(const std::string &name) -> void;
  auto handle_coordinates(const std::vector<uint32_t> &coordinates) -> void;
  auto handle_state(const std::vector<uint32_t> &state) -> void;
  auto handle_remove() -> void;
  auto make_persistent() -> void;
  auto handle_duplicate() -> void;

  auto handle_done() -> void;
  auto handle_clicked(GdkEventButton *bt) -> bool;
  auto show() -> void;
  auto hide() -> void;
  auto get_button_ref() -> Gtk::Button & { return button_; }
  auto get_name() -> std::string & { return name_; }
  auto get_coords() -> std::vector<uint32_t> & { return coordinates_; }

  enum class State {
    ACTIVE = (1 << 0),
    URGENT = (1 << 1),
    HIDDEN = (1 << 2),
    EMPTY = (1 << 3),
  };

 private:
  auto get_icon() -> std::string;

  const Bar &bar_;
  const Json::Value &config_;
  WorkspaceGroup &workspace_group_;

  // wlr stuff
  zext_workspace_handle_v1 *workspace_handle_;
  uint32_t state_ = 0;

  uint32_t id_;
  std::string name_;
  std::vector<uint32_t> coordinates_;
  static std::map<std::string, std::string> icons_map_;
  std::string format_;
  bool with_icon_ = false;
  bool persistent_ = false;

  Gtk::Button button_;
  Gtk::Box content_;
  Gtk::Label label_;
};

class WorkspaceGroup {
 public:
  WorkspaceGroup(const waybar::Bar &bar, Gtk::Box &box, const Json::Value &config,
                 WorkspaceManager &manager, zext_workspace_group_handle_v1 *workspace_group_handle,
                 uint32_t id);
  ~WorkspaceGroup();
  auto update() -> void;

  auto id() const -> uint32_t { return id_; }
  auto is_visible() const -> bool;
  auto remove_workspace(uint32_t id_) -> void;
  auto active_only() const -> bool;
  auto creation_delayed() const -> bool;
  auto workspaces() -> std::vector<std::unique_ptr<Workspace>> & { return workspaces_; }
  auto persistent_workspaces() -> std::vector<std::string> & { return persistent_workspaces_; }

  auto sort_workspaces() -> void;
  auto set_need_to_sort() -> void { need_to_sort = true; }
  auto add_button(Gtk::Button &button) -> void;
  auto remove_button(Gtk::Button &button) -> void;
  auto fill_persistent_workspaces() -> void;
  auto create_persistent_workspaces() -> void;

  // wlr stuff
  auto handle_workspace_create(zext_workspace_handle_v1 *workspace_handle) -> void;
  auto handle_remove() -> void;
  auto handle_output_enter(wl_output *output) -> void;
  auto handle_output_leave() -> void;
  auto handle_done() -> void;
  auto commit() -> void;

 private:
  static uint32_t workspace_global_id;
  const waybar::Bar &bar_;
  Gtk::Box &box_;
  const Json::Value &config_;
  WorkspaceManager &workspace_manager_;

  // wlr stuff
  zext_workspace_group_handle_v1 *workspace_group_handle_;
  wl_output *output_ = nullptr;

  uint32_t id_;
  std::vector<std::unique_ptr<Workspace>> workspaces_;
  bool need_to_sort = false;
  std::vector<std::string> persistent_workspaces_;
  bool persistent_created_ = false;
};

class WorkspaceManager : public AModule {
 public:
  WorkspaceManager(const std::string &id, const waybar::Bar &bar, const Json::Value &config);
  ~WorkspaceManager() override;
  auto update() -> void override;

  auto all_outputs() const -> bool { return all_outputs_; }
  auto active_only() const -> bool { return active_only_; }
  auto workspace_comparator() const
      -> std::function<bool(std::unique_ptr<Workspace> &, std::unique_ptr<Workspace> &)>;
  auto creation_delayed() const -> bool { return creation_delayed_; }

  auto sort_workspaces() -> void;
  auto remove_workspace_group(uint32_t id_) -> void;

  // wlr stuff
  auto register_manager(wl_registry *registry, uint32_t name, uint32_t version) -> void;
  auto handle_workspace_group_create(zext_workspace_group_handle_v1 *workspace_group_handle)
      -> void;
  auto handle_done() -> void;
  auto handle_finished() -> void;
  auto commit() -> void;

 private:
  const waybar::Bar &bar_;
  Gtk::Box box_;
  std::vector<std::unique_ptr<WorkspaceGroup>> groups_;

  // wlr stuff
  zext_workspace_manager_v1 *workspace_manager_ = nullptr;

  static uint32_t group_global_id;

  bool sort_by_name_ = true;
  bool sort_by_coordinates_ = true;
  bool sort_by_number_ = false;
  bool all_outputs_ = false;
  bool active_only_ = false;
  bool creation_delayed_ = false;
};

}  // namespace waybar::modules::wlr
