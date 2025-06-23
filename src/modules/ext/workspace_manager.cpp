#include "modules/ext/workspace_manager.hpp"

#include <gdk/gdkwayland.h>
#include <gtkmm.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include "client.hpp"
#include "gtkmm/widget.h"
#include "modules/ext/workspace_manager_binding.hpp"

namespace waybar::modules::ext {

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
    spdlog::warn("[ext/workspaces]: Prefer sort-by-id instead of sort-by-number");
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

  // setup UI

  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  spdlog::debug("[ext/workspaces]: Workspace manager created");
}

WorkspaceManager::~WorkspaceManager() {
  workspaces_.clear();
  groups_.clear();

  if (ext_manager_ != nullptr) {
    auto *display = Client::inst()->wl_display;
    // Send `stop` request and wait for one roundtrip.
    ext_workspace_manager_v1_stop(ext_manager_);
    wl_display_roundtrip(display);
  }

  if (ext_manager_ != nullptr) {
    spdlog::warn("[ext/workspaces]: Destroying workspace manager before .finished event");
    ext_workspace_manager_v1_destroy(ext_manager_);
  }

  spdlog::debug("[ext/workspaces]: Workspace manager destroyed");
}

void WorkspaceManager::register_manager(wl_registry *registry, uint32_t name, uint32_t version) {
  if (ext_manager_ != nullptr) {
    spdlog::warn("[ext/workspaces]: Register workspace manager again although already registered!");
    return;
  }
  if (version != 1) {
    spdlog::warn("[ext/workspaces]: Using different workspace manager protocol version: {}",
                 version);
  }

  ext_manager_ = workspace_manager_bind(registry, name, version, this);
}

void WorkspaceManager::remove_workspace_group(uint32_t id) {
  const auto it =
      std::find_if(groups_.begin(), groups_.end(), [id](const auto &g) { return g->id() == id; });

  if (it == groups_.end()) {
    spdlog::warn("[ext/workspaces]: Can't find workspace group with id {}", id);
    return;
  }

  groups_.erase(it);
}

void WorkspaceManager::remove_workspace(uint32_t id) {
  const auto it = std::find_if(workspaces_.begin(), workspaces_.end(),
                               [id](const auto &w) { return w->id() == id; });

  if (it == workspaces_.end()) {
    spdlog::warn("[ext/workspaces]: Can't find workspace with id {}", id);
    return;
  }

  workspaces_.erase(it);
}

void WorkspaceManager::handle_workspace_group(ext_workspace_group_handle_v1 *handle) {
  const auto new_id = ++group_global_id;
  groups_.push_back(std::make_unique<WorkspaceGroup>(*this, handle, new_id));
  spdlog::debug("[ext/workspaces]: Workspace group {} created", new_id);
}

void WorkspaceManager::handle_workspace(ext_workspace_handle_v1 *handle) {
  const auto new_id = ++workspace_global_id;
  const auto new_name = std::to_string(++workspace_name);
  workspaces_.push_back(std::make_unique<Workspace>(config_, *this, handle, new_id, new_name));
  set_needs_sorting();
  spdlog::debug("[ext/workspaces]: Workspace {} created", new_id);
}

void WorkspaceManager::handle_done() { dp.emit(); }

void WorkspaceManager::handle_finished() {
  spdlog::debug("[ext/workspaces]: Finishing workspace manager");
  ext_workspace_manager_v1_destroy(ext_manager_);
  ext_manager_ = nullptr;
}

void WorkspaceManager::commit() const { ext_workspace_manager_v1_commit(ext_manager_); }

void WorkspaceManager::update() {
  spdlog::debug("[ext/workspaces]: Updating state");

  if (needs_sorting_) {
    clear_buttons();
    sort_workspaces();
    needs_sorting_ = false;
  }

  update_buttons();
  AModule::update();
}

bool WorkspaceManager::has_button(const Gtk::Button *button) {
  const auto buttons = box_.get_children();
  return std::find(buttons.begin(), buttons.end(), button) != buttons.end();
}

void WorkspaceManager::sort_workspaces() {
  // determine if workspace ID's and names can be sort numerically or literally

  auto is_numeric = [](const std::string &s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
  };

  auto sort_by_workspace_id_numerically =
      std::all_of(workspaces_.begin(), workspaces_.end(),
                  [&](const auto &w) { return is_numeric(w->workspace_id()); });

  auto sort_by_name_numerically = std::all_of(workspaces_.begin(), workspaces_.end(),
                                              [&](const auto &w) { return is_numeric(w->name()); });

  // sort based on configuration setting with sort-by-id as fallback

  std::sort(workspaces_.begin(), workspaces_.end(), [&](const auto &w1, const auto &w2) {
    if (sort_by_id_ || (!sort_by_name_ && !sort_by_coordinates_)) {
      if (w1->workspace_id() == w2->workspace_id()) {
        return w1->id() < w2->id();
      }
      if (sort_by_workspace_id_numerically) {
        // the idea is that phonetic compare can be applied just to numbers
        // with same number of digits
        return w1->workspace_id().size() < w2->workspace_id().size() ||
               (w1->workspace_id().size() == w2->workspace_id().size() &&
                w1->workspace_id() < w2->workspace_id());
      }
      return w1->workspace_id() < w2->workspace_id();
    }

    if (sort_by_name_) {
      if (w1->name() == w2->name()) {
        return w1->id() < w2->id();
      }
      if (sort_by_name_numerically) {
        // see above about numeric sorting
        return w1->name().size() < w2->name().size() ||
               (w1->name().size() == w2->name().size() && w1->name() < w2->name());
      }
      return w1->name() < w2->name();
    }

    if (sort_by_coordinates_) {
      if (w1->coordinates() == w2->coordinates()) {
        return w1->id() < w2->id();
      }
      return w1->coordinates() < w2->coordinates();
    }

    return w1->id() < w2->id();
  });
}

void WorkspaceManager::clear_buttons() {
  for (const auto &workspace : workspaces_) {
    if (has_button(&workspace->button())) {
      box_.remove(workspace->button());
    }
  }
}

void WorkspaceManager::update_buttons() {
  const auto *output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());

  // go through all workspace

  for (const auto &workspace : workspaces_) {
    const bool workspace_on_any_group_for_output =
        std::any_of(groups_.begin(), groups_.end(), [&](const auto &group) {
          const bool group_on_output = group->has_output(output) || all_outputs_;
          const bool workspace_on_group = group->has_workspace(workspace->handle());
          return group_on_output && workspace_on_group;
        });
    const bool bar_contains_button = has_button(&workspace->button());

    // add or remove buttons if needed, update button state

    if (workspace_on_any_group_for_output) {
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

// WorkspaceGroup

WorkspaceGroup::WorkspaceGroup(WorkspaceManager &manager, ext_workspace_group_handle_v1 *handle,
                               uint32_t id)
    : workspaces_manager_(manager), ext_handle_(handle), id_(id) {
  add_workspace_group_listener(ext_handle_, this);
}

WorkspaceGroup::~WorkspaceGroup() {
  if (ext_handle_ != nullptr) {
    ext_workspace_group_handle_v1_destroy(ext_handle_);
  }
  spdlog::debug("[ext/workspaces]: Workspace group {} destroyed", id_);
}

bool WorkspaceGroup::has_output(const wl_output *output) {
  return std::find(outputs_.begin(), outputs_.end(), output) != outputs_.end();
}

bool WorkspaceGroup::has_workspace(const ext_workspace_handle_v1 *workspace) {
  return std::find(workspaces_.begin(), workspaces_.end(), workspace) != workspaces_.end();
}

void WorkspaceGroup::handle_capabilities(uint32_t capabilities) {
  spdlog::debug("[ext/workspaces]:     Capabilities for workspace group {}:", id_);
  if ((capabilities & EXT_WORKSPACE_GROUP_HANDLE_V1_GROUP_CAPABILITIES_CREATE_WORKSPACE) ==
      capabilities) {
    spdlog::debug("[ext/workspaces]:     - create-workspace");
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
  spdlog::debug("[ext/workspaces]: Removing workspace group {}", id_);
  workspaces_manager_.remove_workspace_group(id_);
}

// Workspace

Workspace::Workspace(const Json::Value &config, WorkspaceManager &manager,
                     ext_workspace_handle_v1 *handle, uint32_t id, const std::string &name)
    : workspace_manager_(manager), ext_handle_(handle), id_(id), workspace_id_(name), name_(name) {
  add_workspace_listener(ext_handle_, this);

  // parse configuration

  const auto &config_active_only = config["active-only"];
  if (config_active_only.isBool()) {
    active_only_ = config_active_only.asBool();
  }

  const auto &config_ignore_hidden = config["ignore-hidden"];
  if (config_ignore_hidden.isBool()) {
    ignore_hidden_ = config_ignore_hidden.asBool();
  }

  const auto &config_format = config["format"];
  format_ = config_format.isString() ? config_format.asString() : "{name}";
  with_icon_ = format_.find("{icon}") != std::string::npos;

  if (with_icon_ && icon_map_.empty()) {
    const auto &format_icons = config["format-icons"];
    for (auto &n : format_icons.getMemberNames()) {
      icon_map_.emplace(n, format_icons[n].asString());
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

Workspace::~Workspace() {
  if (ext_handle_ != nullptr) {
    ext_workspace_handle_v1_destroy(ext_handle_);
  }
  spdlog::debug("[ext/workspaces]: Workspace {} destroyed", id_);
}

void Workspace::update() {
  if (!needs_updating_) {
    return;
  }

  // update style and visibility

  const auto style_context = button_.get_style_context();
  style_context->remove_class("active");
  style_context->remove_class("urgent");
  style_context->remove_class("hidden");

  if (has_state(EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE)) {
    button_.set_visible(true);
    style_context->add_class("active");
  }
  if (has_state(EXT_WORKSPACE_HANDLE_V1_STATE_URGENT)) {
    button_.set_visible(true);
    style_context->add_class("urgent");
  }
  if (has_state(EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN)) {
    button_.set_visible(!active_only_ && !ignore_hidden_);
    style_context->add_class("hidden");
  }
  if (state_ == 0) {
    button_.set_visible(!active_only_);
  }

  // update label
  label_.set_markup(fmt::format(fmt::runtime(format_), fmt::arg("name", name_),
                                fmt::arg("id", workspace_id_),
                                fmt::arg("icon", with_icon_ ? icon() : "")));

  needs_updating_ = false;
}

void Workspace::handle_id(const std::string &id) {
  spdlog::debug("[ext/workspaces]:     ID for workspace {}: {}", id_, id);
  workspace_id_ = id;
  needs_updating_ = true;
  workspace_manager_.set_needs_sorting();
}

void Workspace::handle_name(const std::string &name) {
  spdlog::debug("[ext/workspaces]:     Name for workspace {}: {}", id_, name);
  name_ = name;
  needs_updating_ = true;
  workspace_manager_.set_needs_sorting();
}

void Workspace::handle_coordinates(const std::vector<uint32_t> &coordinates) {
  coordinates_ = coordinates;
  needs_updating_ = true;
  workspace_manager_.set_needs_sorting();
}

void Workspace::handle_state(uint32_t state) {
  state_ = state;
  needs_updating_ = true;
}

void Workspace::handle_capabilities(uint32_t capabilities) {
  spdlog::debug("[ext/workspaces]:     Capabilities for workspace {}:", id_);
  if ((capabilities & EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE) == capabilities) {
    spdlog::debug("[ext/workspaces]:     - activate");
  }
  if ((capabilities & EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_DEACTIVATE) == capabilities) {
    spdlog::debug("[ext/workspaces]:     - deactivate");
  }
  if ((capabilities & EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_REMOVE) == capabilities) {
    spdlog::debug("[ext/workspaces]:     - remove");
  }
  if ((capabilities & EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ASSIGN) == capabilities) {
    spdlog::debug("[ext/workspaces]:     - assign");
  }
  needs_updating_ = true;
}

void Workspace::handle_removed() {
  spdlog::debug("[ext/workspaces]: Removing workspace {}", id_);
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
    spdlog::warn("[ext/workspaces]: Unknown action {}", action);
  }
  workspace_manager_.commit();
  return true;
}

std::string Workspace::icon() {
  if (has_state(EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE)) {
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

}  // namespace waybar::modules::ext
