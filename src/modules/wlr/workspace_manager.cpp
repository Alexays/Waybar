#include "modules/wlr/workspace_manager.hpp"

#include <gdk/gdkwayland.h>
#include <gtkmm.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include "client.hpp"
#include "gtkmm/widget.h"
#include "modules/wlr/workspace_manager_binding.hpp"

namespace waybar::modules::wlr {

// WorkspaceManager

uint32_t WorkspaceManager::group_global_id = 0;
uint32_t WorkspaceManager::workspace_global_id = 0;
std::map<std::string, std::string> Workspace::icon_map_;

WorkspaceManager::WorkspaceManager(const std::string &id, const waybar::Bar &bar,
                                   const Json::Value &config)
    : waybar::AModule(config, "workspaces", id, false, false), bar_(bar), box_(bar.orientation, 0) {
  add_registry_listener(this);

  // parse configuration

  const auto config_sort_by_number = config_["sort-by-number"];
  if (config_sort_by_number.isBool()) {
    spdlog::warn("[wlr/workspaces]: Prefer sort-by-id instead of sort-by-number");
    sort_by_id_ = config_sort_by_number.asBool();
  }

  const auto config_sort_by_id = config_["sort-by-id"];
  if (config_sort_by_id.isBool()) {
    sort_by_id_ = config_sort_by_id.asBool();
  }

  const auto config_sort_by_name = config_["sort-by-name"];
  if (config_sort_by_name.isBool()) {
    sort_by_name_ = config_sort_by_name.asBool();
  }

  const auto config_sort_by_coordinates = config_["sort-by-coordinates"];
  if (config_sort_by_coordinates.isBool()) {
    sort_by_coordinates_ = config_sort_by_coordinates.asBool();
  }

  const auto config_all_outputs = config_["all-outputs"];
  if (config_all_outputs.isBool()) {
    all_outputs_ = config_all_outputs.asBool();
  }

  const auto config_active_only = config_["active-only"];
  if (config_active_only.isBool()) {
    active_only_ = config_active_only.asBool();
  }

  // setup UI

  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);
}

void WorkspaceManager::register_manager(wl_registry *registry, uint32_t name, uint32_t version) {
  if (ext_manager_ != nullptr) {
    spdlog::warn("[wlr/workspaces]: Register workspace manager again although already registered!");
    return;
  }
  if (version != 1) {
    spdlog::warn("[wlr/workspaces]: Using different workspace manager protocol version: {}",
                 version);
  }

  ext_manager_ = workspace_manager_bind(registry, name, version, this);
}

void WorkspaceManager::remove_workspace(uint32_t id) {
  const auto it = std::find_if(workspaces_.begin(), workspaces_.end(),
                               [id](const auto &w) { return w->id() == id; });

  if (it == workspaces_.end()) {
    spdlog::warn("[wlr/workspaces]: Can't find workspace with id {}", id);
    return;
  }

  workspaces_.erase(it);
}

void WorkspaceManager::remove_workspace_group(uint32_t id) {
  const auto it =
      std::find_if(groups_.begin(), groups_.end(), [id](const auto &g) { return g->id() == id; });

  if (it == groups_.end()) {
    spdlog::warn("[wlr/workspaces]: Can't find group with id {}", id);
    return;
  }

  groups_.erase(it);
}

void WorkspaceManager::handle_workspace_group(ext_workspace_group_handle_v1 *handle) {
  const auto new_id = ++group_global_id;
  groups_.push_back(std::make_unique<WorkspaceGroup>(*this, handle, new_id));
  spdlog::debug("[wlr/workspaces]: Workspace group {} created", new_id);
}

void WorkspaceManager::handle_workspace(ext_workspace_handle_v1 *handle) {
  const auto new_id = ++workspace_global_id;
  workspaces_.push_back(std::make_unique<Workspace>(config_, *this, handle, new_id));
  spdlog::debug("[wlr/workspaces]: Workspace {} created", new_id);
}

void WorkspaceManager::handle_done() { dp.emit(); }

void WorkspaceManager::handle_finished() {
  ext_workspace_manager_v1_destroy(ext_manager_);
  ext_manager_ = nullptr;
}

void WorkspaceManager::commit() const { ext_workspace_manager_v1_commit(ext_manager_); }

void WorkspaceManager::update() {
  spdlog::debug("[wlr/workspaces]: Updating state");
  const auto *output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());

  // sort workspaces if needed

  if (sort_by_id_ || sort_by_name_ || sort_by_coordinates_) {
    std::sort(workspaces_.begin(), workspaces_.end(), [&](const auto &w1, const auto &w2) {
      if (sort_by_id_) {
        return w1->workspace_id() < w2->workspace_id();
      }
      if (sort_by_name_) {
        return w1->name() < w2->name();
      }
      if (sort_by_coordinates_) {
        return w1->coordinates() < w2->coordinates();
      }
      return w1->id() < w2->id();
    });
  }

  // add or remove buttons if needed, update button state

  for (const auto &group : groups_) {
    const bool group_on_output = group->has_output(output) || all_outputs_;

    for (const auto &workspace : workspaces_) {
      const bool workspace_on_group = group->has_workspace(workspace->handle());
      const bool valid_workspace_state = (workspace->is_active() && active_only_) || !active_only_;

      const bool show_button = group_on_output && workspace_on_group && valid_workspace_state;
      const bool bar_contains_button = has_button(&workspace->button());

      if (show_button) {
        if (!bar_contains_button) {
          // add button to bar
          box_.pack_start(workspace->button(), false, false);
          workspace->button().show_all();
        }
        workspace->update();
      } else {
        if (bar_contains_button) {
          // remove button from bar
          box_.remove(workspace->button());
        }
      }
    }
  }

  AModule::update();
}

bool WorkspaceManager::has_button(const Gtk::Button *button) {
  const auto buttons = box_.get_children();
  return std::find(buttons.begin(), buttons.end(), button) != buttons.end();
}

// WorkspaceGroup

WorkspaceGroup::WorkspaceGroup(WorkspaceManager &manager, ext_workspace_group_handle_v1 *handle,
                               uint32_t id)
    : workspaces_manager_(manager), ext_handle_(handle), id_(id) {
  add_workspace_group_listener(ext_handle_, this);
}

bool WorkspaceGroup::has_output(const wl_output *output) {
  return std::find(outputs_.begin(), outputs_.end(), output) != outputs_.end();
}

bool WorkspaceGroup::has_workspace(const ext_workspace_handle_v1 *workspace) {
  return std::find(workspaces_.begin(), workspaces_.end(), workspace) != workspaces_.end();
}

void WorkspaceGroup::handle_capabilities(uint32_t capabilities) {
  spdlog::debug("[wlr/workspaces]: capabilities for workspace group {}:", id_);
  if ((capabilities & EXT_WORKSPACE_GROUP_HANDLE_V1_GROUP_CAPABILITIES_CREATE_WORKSPACE) ==
      capabilities) {
    spdlog::debug("[wlr/workspaces]:     create-workspace");
  }
}

void WorkspaceGroup::handle_output_enter(wl_output *output) { outputs_.push_back(output); }

void WorkspaceGroup::handle_output_leave(wl_output *output) {
  const auto it = std::find(outputs_.begin(), outputs_.end(), output);
  if (it != outputs_.end()) {
    outputs_.erase(it);
  }
}

void WorkspaceGroup::handle_workspace_enter(ext_workspace_handle_v1 *handle) {
  workspaces_.push_back(handle);
}

void WorkspaceGroup::handle_workspace_leave(ext_workspace_handle_v1 *handle) {
  const auto it = std::find(workspaces_.begin(), workspaces_.end(), handle);
  if (it != workspaces_.end()) {
    workspaces_.erase(it);
  }
}

void WorkspaceGroup::handle_removed() {
  spdlog::debug("[wlr/workspaces]: Removing workspace group {}", id_);
  ext_workspace_group_handle_v1_destroy(ext_handle_);
  ext_handle_ = nullptr;

  workspaces_manager_.remove_workspace_group(id_);
}

// Workspace

Workspace::Workspace(const Json::Value &config, WorkspaceManager &manager,
                     ext_workspace_handle_v1 *handle, uint32_t id)
    : workspace_manager_(manager),
      ext_handle_(handle),
      id_(id),
      workspace_id_(std::to_string(id)),
      name_(std::to_string(id)) {
  add_workspace_listener(ext_handle_, this);

  // parse configuration

  const auto &config_format = config["format"];
  format_ = config_format.isString() ? config_format.asString() : "{name}";
  with_icon_ = format_.find("{icon}") != std::string::npos;

  if (with_icon_ && icon_map_.empty()) {
    const auto &format_icons = config["format-icons"];
    for (auto &name : format_icons.getMemberNames()) {
      icon_map_.emplace(name, format_icons[name].asString());
    }
  }

  const bool config_on_click = config["on-click"].isString();
  if (config_on_click) {
    on_click_action_ = config["on-click"].asString();
  }
  const bool config_on_click_middle = config["on-click-middle"].isString();
  if (config_on_click_middle) {
    on_click_middle_action_ = config["on-click"].asString();
  }
  const bool config_on_click_right = config["on-click-right"].isString();
  if (config_on_click_right) {
    on_click_right_action_ = config["on-click"].asString();
  }

  // setup UI

  if (config_on_click || config_on_click_middle || config_on_click_right) {
    button_.add_events(Gdk::BUTTON_PRESS_MASK);
    button_.signal_button_press_event().connect(sigc::mem_fun(*this, &Workspace::handle_clicked),
                                                false);
  }

  button_.set_relief(Gtk::RELIEF_NONE);
  content_.set_center_widget(label_);
  button_.add(content_);
}

bool Workspace::is_active() const {
  return (state_ & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE) == EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;
}

void Workspace::update() {
  if (!dirty_) {
    return;
  }

  // update style
  const auto style_context = button_.get_style_context();
  style_context->remove_class("active");
  style_context->remove_class("urgent");
  style_context->remove_class("hidden");

  if ((state_ & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE) == EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE) {
    style_context->add_class("active");
  }
  if ((state_ & EXT_WORKSPACE_HANDLE_V1_STATE_URGENT) == EXT_WORKSPACE_HANDLE_V1_STATE_URGENT) {
    style_context->add_class("urgent");
  }
  if ((state_ & EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN) == EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN) {
    style_context->add_class("hidden");
  }

  // update label
  label_.set_markup(fmt::format(fmt::runtime(format_), fmt::arg("name", name_),
                                fmt::arg("id", workspace_id_),
                                fmt::arg("icon", with_icon_ ? icon() : "")));

  dirty_ = false;
}

void Workspace::handle_id(const std::string &id) {
  workspace_id_ = id;
  dirty_ = true;
}

void Workspace::handle_name(const std::string &name) {
  name_ = name;
  dirty_ = true;
}

void Workspace::handle_coordinates(const std::vector<uint32_t> &coordinates) {
  coordinates_ = coordinates;
  dirty_ = true;
}

void Workspace::handle_state(uint32_t state) {
  state_ = state;
  dirty_ = true;
}

void Workspace::handle_capabilities(uint32_t capabilities) {
  spdlog::debug("[wlr/workspaces]: capabilities for workspace {}:", id_);
  if ((capabilities & EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE) == capabilities) {
    spdlog::debug("[wlr/workspaces]:     activate");
  }
  if ((capabilities & EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_DEACTIVATE) == capabilities) {
    spdlog::debug("[wlr/workspaces]:     deactivate");
  }
  if ((capabilities & EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_REMOVE) == capabilities) {
    spdlog::debug("[wlr/workspaces]:     remove");
  }
  if ((capabilities & EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ASSIGN) == capabilities) {
    spdlog::debug("[wlr/workspaces]:     assign");
  }
  dirty_ = true;
}

void Workspace::handle_removed() {
  spdlog::debug("[wlr/workspaces]: Removing workspace {}", id_);
  ext_workspace_handle_v1_destroy(ext_handle_);
  ext_handle_ = nullptr;

  workspace_manager_.remove_workspace(id_);
}

bool Workspace::handle_clicked(const GdkEventButton *button) const {
  std::string action;
  if (button->button == GDK_BUTTON_PRIMARY) {
    action = on_click_action_;
  } else if (button->button == GDK_BUTTON_MIDDLE) {
    action = on_click_middle_action_;
  } else if (button->button == GDK_BUTTON_SECONDARY) {
    action = on_click_right_action_;
  }

  if (action.empty()) {
    return true;
  }

  if (action == "activate") {
    ext_workspace_handle_v1_activate(ext_handle_);
  } else if (action == "close") {
    ext_workspace_handle_v1_remove(ext_handle_);
  } else {
    spdlog::warn("[wlr/workspaces]: Unknown action {}", action);
  }
  workspace_manager_.commit();
  return true;
}

std::string Workspace::icon() {
  if (is_active()) {
    const auto active_icon_it = icon_map_.find("active");
    if (active_icon_it != icon_map_.end()) {
      return active_icon_it->second;
    }
  }

  const auto named_icon_it = icon_map_.find(name_);
  if (named_icon_it != icon_map_.end()) {
    return named_icon_it->second;
  }

  const auto default_icon_it = icon_map_.find("default");
  if (default_icon_it != icon_map_.end()) {
    return default_icon_it->second;
  }

  return name_;
}

}  // namespace waybar::modules::wlr
