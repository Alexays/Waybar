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
#include "ext-workspace-v1-client-protocol.h"

namespace waybar::modules::wlr {

class WorkspaceGroup;
class Workspace;

class WorkspaceManager final : public AModule {
 public:
  WorkspaceManager(const std::string &id, const waybar::Bar &bar, const Json::Value &config);
  ~WorkspaceManager() override;
  void register_manager(wl_registry *registry, uint32_t name, uint32_t version);
  void remove_workspace_group(uint32_t id);
  void remove_workspace(uint32_t id);
  void set_needs_sorting() { needs_sorting_ = true; }

  // wlr events
  void handle_workspace_group(ext_workspace_group_handle_v1 *handle);
  void handle_workspace(ext_workspace_handle_v1 *handle);
  void handle_done();
  void handle_finished();

  // wlr requests
  void commit() const;

 private:
  void update() override;
  bool has_button(const Gtk::Button *button);
  void sort_workspaces();
  void clear_buttons();
  void update_buttons();

  static uint32_t group_global_id;
  static uint32_t workspace_global_id;
  uint32_t workspace_name = 0;

  bool sort_by_id_ = false;
  bool sort_by_name_ = true;
  bool sort_by_coordinates_ = false;
  bool active_only_ = false;
  bool all_outputs_ = false;

  const waybar::Bar &bar_;
  Gtk::Box box_;

  ext_workspace_manager_v1 *ext_manager_ = nullptr;
  std::vector<std::unique_ptr<WorkspaceGroup>> groups_;
  std::vector<std::unique_ptr<Workspace>> workspaces_;

  bool needs_sorting_ = false;
};

class WorkspaceGroup {
 public:
  WorkspaceGroup(WorkspaceManager &manager, ext_workspace_group_handle_v1 *handle, uint32_t id);
  ~WorkspaceGroup();

  u_int32_t id() const { return id_; }
  bool has_output(const wl_output *output);
  bool has_workspace(const ext_workspace_handle_v1 *workspace);

  // wlr events
  void handle_capabilities(uint32_t capabilities);
  void handle_output_enter(wl_output *output);
  void handle_output_leave(wl_output *output);
  void handle_workspace_enter(ext_workspace_handle_v1 *handle);
  void handle_workspace_leave(ext_workspace_handle_v1 *handle);
  void handle_removed();

 private:
  WorkspaceManager &workspaces_manager_;
  ext_workspace_group_handle_v1 *ext_handle_;
  uint32_t id_;
  std::vector<wl_output *> outputs_;
  std::vector<ext_workspace_handle_v1 *> workspaces_;
};

class Workspace {
 public:
  Workspace(const Json::Value &config, WorkspaceManager &manager, ext_workspace_handle_v1 *handle,
            uint32_t id, const std::string &name);
  ~Workspace();

  ext_workspace_handle_v1 *handle() const { return ext_handle_; }
  u_int32_t id() const { return id_; }
  std::string &workspace_id() { return workspace_id_; }
  std::string &name() { return name_; }
  std::vector<u_int32_t> &coordinates() { return coordinates_; }
  Gtk::Button &button() { return button_; }
  bool is_active() const;
  bool is_urgent() const;
  bool is_hidden() const;
  void update();

  // wlr events
  void handle_id(const std::string &id);
  void handle_name(const std::string &name);
  void handle_coordinates(const std::vector<uint32_t> &coordinates);
  void handle_state(uint32_t state);
  void handle_capabilities(uint32_t capabilities);
  void handle_removed();

  // gdk events
  bool handle_clicked(const GdkEventButton *button) const;

 private:
  std::string icon();

  WorkspaceManager &workspace_manager_;
  ext_workspace_handle_v1 *ext_handle_ = nullptr;
  uint32_t id_;
  uint32_t state_ = 0;
  std::string workspace_id_;
  std::string name_;
  std::vector<uint32_t> coordinates_;

  std::string format_;
  bool with_icon_ = false;
  static std::map<std::string, std::string> icon_map_;
  std::string on_click_action_;
  std::string on_click_middle_action_;
  std::string on_click_right_action_;

  Gtk::Button button_;
  Gtk::Box content_;
  Gtk::Label label_;

  bool needs_updating_ = false;
};

}  // namespace waybar::modules::wlr
