#include "modules/wlr/workspace_manager.hpp"

#include <gdk/gdkwayland.h>
#include <gtkmm.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "client.hpp"
#include "gtkmm/widget.h"
#include "modules/wlr/workspace_manager_binding.hpp"

namespace waybar::modules::wlr {

uint32_t WorkspaceGroup::workspace_global_id = 0;
uint32_t WorkspaceManager::group_global_id = 0;
std::map<std::string, std::string> Workspace::icons_map_;

WorkspaceManager::WorkspaceManager(const std::string &id, const waybar::Bar &bar,
                                   const Json::Value &config)
    : waybar::AModule(config, "workspaces", id, false, false),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  auto config_sort_by_name = config_["sort-by-name"];
  if (config_sort_by_name.isBool()) {
    sort_by_name_ = config_sort_by_name.asBool();
  }

  auto config_sort_by_coordinates = config_["sort-by-coordinates"];
  if (config_sort_by_coordinates.isBool()) {
    sort_by_coordinates_ = config_sort_by_coordinates.asBool();
  }

  auto config_sort_by_number = config_["sort-by-number"];
  if (config_sort_by_number.isBool()) {
    sort_by_number_ = config_sort_by_number.asBool();
  }

  auto config_all_outputs = config_["all-outputs"];
  if (config_all_outputs.isBool()) {
    all_outputs_ = config_all_outputs.asBool();
  }

  auto config_active_only = config_["active-only"];
  if (config_active_only.isBool()) {
    active_only_ = config_active_only.asBool();
    creation_delayed_ = active_only_;
  }

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

auto WorkspaceManager::workspace_comparator() const
    -> std::function<bool(std::unique_ptr<Workspace> &, std::unique_ptr<Workspace> &)> {
  return [=, this](std::unique_ptr<Workspace> &lhs, std::unique_ptr<Workspace> &rhs) {
    auto is_name_less = lhs->get_name() < rhs->get_name();
    auto is_name_eq = lhs->get_name() == rhs->get_name();
    auto is_coords_less = lhs->get_coords() < rhs->get_coords();

    if (sort_by_number_) {
      try {
        auto is_number_less = std::stoi(lhs->get_name()) < std::stoi(rhs->get_name());
        return is_number_less;
      } catch (const std::invalid_argument &) {
      }
    }

    if (sort_by_name_) {
      if (sort_by_coordinates_) {
        return is_name_eq ? is_coords_less : is_name_less;
      } else {
        return is_name_less;
      }
    }

    if (sort_by_coordinates_) {
      return is_coords_less;
    }

    return lhs->id() < rhs->id();
  };
}

auto WorkspaceManager::sort_workspaces() -> void {
  std::vector<std::reference_wrapper<std::unique_ptr<Workspace>>> all_workspaces;
  for (auto &group : groups_) {
    auto &group_workspaces = group->workspaces();
    all_workspaces.reserve(all_workspaces.size() +
                           std::distance(group_workspaces.begin(), group_workspaces.end()));
    if (!active_only()) {
      all_workspaces.insert(all_workspaces.end(), group_workspaces.begin(), group_workspaces.end());
      continue;
    }

    for (auto &workspace : group_workspaces) {
      if (!workspace->is_active()) {
        continue;
      }

      all_workspaces.push_back(workspace);
    }
  }

  std::sort(all_workspaces.begin(), all_workspaces.end(), workspace_comparator());
  for (size_t i = 0; i < all_workspaces.size(); ++i) {
    box_.reorder_child(all_workspaces[i].get()->get_button_ref(), i);
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
    zext_workspace_group_handle_v1 *workspace_group_handle) -> void {
  auto new_id = ++group_global_id;
  groups_.push_back(
      std::make_unique<WorkspaceGroup>(bar_, box_, config_, *this, workspace_group_handle, new_id));
  spdlog::debug("Workspace group {} created", new_id);
}

auto WorkspaceManager::handle_finished() -> void {
  zext_workspace_manager_v1_destroy(workspace_manager_);
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
  if (creation_delayed()) {
    creation_delayed_ = false;
    sort_workspaces();
  }
  AModule::update();
}

WorkspaceManager::~WorkspaceManager() {
  if (!workspace_manager_) {
    return;
  }

  wl_display *display = Client::inst()->wl_display;

  // Send `stop` request and wait for one roundtrip. This is not quite correct as
  // the protocol encourages us to wait for the .finished event, but it should work
  // with wlroots workspace manager implementation.
  zext_workspace_manager_v1_stop(workspace_manager_);
  wl_display_roundtrip(display);

  // If the .finished handler is still not executed, destroy the workspace manager here.
  if (workspace_manager_) {
    spdlog::warn("Foreign toplevel manager destroyed before .finished event");
    zext_workspace_manager_v1_destroy(workspace_manager_);
    workspace_manager_ = nullptr;
  }
}

auto WorkspaceManager::remove_workspace_group(uint32_t id) -> void {
  auto it = std::find_if(groups_.begin(), groups_.end(),
                         [id](const std::unique_ptr<WorkspaceGroup> &g) { return g->id() == id; });

  if (it == groups_.end()) {
    spdlog::warn("Can't find group with id {}", id);
    return;
  }

  groups_.erase(it);
}
auto WorkspaceManager::commit() -> void { zext_workspace_manager_v1_commit(workspace_manager_); }

WorkspaceGroup::WorkspaceGroup(const Bar &bar, Gtk::Box &box, const Json::Value &config,
                               WorkspaceManager &manager,
                               zext_workspace_group_handle_v1 *workspace_group_handle, uint32_t id)
    : bar_(bar),
      box_(box),
      config_(config),
      workspace_manager_(manager),
      workspace_group_handle_(workspace_group_handle),
      id_(id) {
  add_workspace_group_listener(workspace_group_handle, this);
}

auto WorkspaceGroup::fill_persistent_workspaces() -> void {
  if (config_["persistent_workspaces"].isObject() && !workspace_manager_.all_outputs()) {
    const Json::Value &p_workspaces = config_["persistent_workspaces"];
    const std::vector<std::string> p_workspaces_names = p_workspaces.getMemberNames();

    for (const std::string &p_w_name : p_workspaces_names) {
      const Json::Value &p_w = p_workspaces[p_w_name];
      if (p_w.isArray() && !p_w.empty()) {
        // Adding to target outputs
        for (const Json::Value &output : p_w) {
          if (output.asString() == bar_.output->name) {
            persistent_workspaces_.push_back(p_w_name);
            break;
          }
        }
      } else {
        // Adding to all outputs
        persistent_workspaces_.push_back(p_w_name);
      }
    }
  }
}

auto WorkspaceGroup::create_persistent_workspaces() -> void {
  for (const std::string &p_w_name : persistent_workspaces_) {
    auto new_id = ++workspace_global_id;
    workspaces_.push_back(
        std::make_unique<Workspace>(bar_, config_, *this, nullptr, new_id, p_w_name));
    spdlog::debug("Workspace {} created", new_id);
  }
}

auto WorkspaceGroup::active_only() const -> bool { return workspace_manager_.active_only(); }
auto WorkspaceGroup::creation_delayed() const -> bool {
  return workspace_manager_.creation_delayed();
}

auto WorkspaceGroup::add_button(Gtk::Button &button) -> void {
  box_.pack_start(button, false, false);
}

WorkspaceGroup::~WorkspaceGroup() {
  if (!workspace_group_handle_) {
    return;
  }

  zext_workspace_group_handle_v1_destroy(workspace_group_handle_);
  workspace_group_handle_ = nullptr;
}

auto WorkspaceGroup::handle_workspace_create(zext_workspace_handle_v1 *workspace) -> void {
  auto new_id = ++workspace_global_id;
  workspaces_.push_back(std::make_unique<Workspace>(bar_, config_, *this, workspace, new_id, ""));
  spdlog::debug("Workspace {} created", new_id);
  if (!persistent_created_) {
    fill_persistent_workspaces();
    create_persistent_workspaces();
    persistent_created_ = true;
  }
}

auto WorkspaceGroup::handle_remove() -> void {
  zext_workspace_group_handle_v1_destroy(workspace_group_handle_);
  workspace_group_handle_ = nullptr;
  workspace_manager_.remove_workspace_group(id_);
}

auto WorkspaceGroup::handle_output_enter(wl_output *output) -> void {
  spdlog::debug("Output {} assigned to {} group", (void *)output, id_);
  output_ = output;

  if (!is_visible() || workspace_manager_.creation_delayed()) {
    return;
  }

  for (auto &workspace : workspaces_) {
    add_button(workspace->get_button_ref());
  }
}

auto WorkspaceGroup::is_visible() const -> bool {
  return output_ != nullptr &&
         (workspace_manager_.all_outputs() ||
          output_ == gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj()));
}

auto WorkspaceGroup::handle_output_leave() -> void {
  spdlog::debug("Output {} remove from {} group", (void *)output_, id_);
  output_ = nullptr;

  if (output_ != gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj())) {
    return;
  }

  for (auto &workspace : workspaces_) {
    remove_button(workspace->get_button_ref());
  }
}

auto WorkspaceGroup::update() -> void {
  for (auto &workspace : workspaces_) {
    if (workspace_manager_.creation_delayed()) {
      add_button(workspace->get_button_ref());
      if (is_visible() && (workspace->is_active() || workspace->is_urgent())) {
        workspace->show();
      }
    }

    workspace->update();
  }
}

auto WorkspaceGroup::remove_workspace(uint32_t id) -> void {
  auto it = std::find_if(workspaces_.begin(), workspaces_.end(),
                         [id](const std::unique_ptr<Workspace> &w) { return w->id() == id; });

  if (it == workspaces_.end()) {
    spdlog::warn("Can't find workspace with id {}", id);
    return;
  }

  workspaces_.erase(it);
}

auto WorkspaceGroup::handle_done() -> void {
  need_to_sort = false;
  if (!is_visible()) {
    return;
  }

  for (auto &workspace : workspaces_) {
    workspace->handle_done();
  }

  if (creation_delayed()) {
    return;
  }

  if (!workspace_manager_.all_outputs()) {
    sort_workspaces();
  } else {
    workspace_manager_.sort_workspaces();
  }
}

auto WorkspaceGroup::commit() -> void { workspace_manager_.commit(); }

auto WorkspaceGroup::sort_workspaces() -> void {
  std::sort(workspaces_.begin(), workspaces_.end(), workspace_manager_.workspace_comparator());
  for (size_t i = 0; i < workspaces_.size(); ++i) {
    box_.reorder_child(workspaces_[i]->get_button_ref(), i);
  }
}

auto WorkspaceGroup::remove_button(Gtk::Button &button) -> void { box_.remove(button); }

Workspace::Workspace(const Bar &bar, const Json::Value &config, WorkspaceGroup &workspace_group,
                     zext_workspace_handle_v1 *workspace, uint32_t id, std::string name)
    : bar_(bar),
      config_(config),
      workspace_group_(workspace_group),
      workspace_handle_(workspace),
      id_(id),
      name_(name) {
  if (workspace) {
    add_workspace_listener(workspace, this);
  } else {
    state_ = (uint32_t)State::EMPTY;
  }

  auto config_format = config["format"];

  format_ = config_format.isString() ? config_format.asString() : "{name}";
  with_icon_ = format_.find("{icon}") != std::string::npos;

  if (with_icon_ && icons_map_.empty()) {
    auto format_icons = config["format-icons"];
    for (auto &name : format_icons.getMemberNames()) {
      icons_map_.emplace(name, format_icons[name].asString());
    }
  }

  /* Handle click events if configured */
  if (config_["on-click"].isString() || config_["on-click-middle"].isString() ||
      config_["on-click-right"].isString()) {
    button_.add_events(Gdk::BUTTON_PRESS_MASK);
    button_.signal_button_press_event().connect(sigc::mem_fun(*this, &Workspace::handle_clicked),
                                                false);
  }

  button_.set_relief(Gtk::RELIEF_NONE);
  content_.set_center_widget(label_);
  button_.add(content_);

  if (!workspace_group.is_visible()) {
    return;
  }

  workspace_group.add_button(button_);
  button_.show_all();
}

Workspace::~Workspace() {
  workspace_group_.remove_button(button_);
  if (!workspace_handle_) {
    return;
  }

  zext_workspace_handle_v1_destroy(workspace_handle_);
  workspace_handle_ = nullptr;
}

auto Workspace::update() -> void {
  label_.set_markup(fmt::format(fmt::runtime(format_), fmt::arg("name", name_),
                                fmt::arg("icon", with_icon_ ? get_icon() : "")));
}

auto Workspace::handle_state(const std::vector<uint32_t> &state) -> void {
  state_ = 0;
  for (auto state_entry : state) {
    switch (state_entry) {
      case ZEXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE:
        state_ |= (uint32_t)State::ACTIVE;
        break;
      case ZEXT_WORKSPACE_HANDLE_V1_STATE_URGENT:
        state_ |= (uint32_t)State::URGENT;
        break;
      case ZEXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN:
        state_ |= (uint32_t)State::HIDDEN;
        break;
    }
  }
}

auto Workspace::handle_remove() -> void {
  if (workspace_handle_) {
    zext_workspace_handle_v1_destroy(workspace_handle_);
    workspace_handle_ = nullptr;
  }
  if (!persistent_) {
    workspace_group_.remove_workspace(id_);
  } else {
    state_ = (uint32_t)State::EMPTY;
  }
}

auto add_or_remove_class(Glib::RefPtr<Gtk::StyleContext> context, bool condition,
                         const std::string &class_name) {
  if (condition) {
    context->add_class(class_name);
  } else {
    context->remove_class(class_name);
  }
}

auto Workspace::handle_done() -> void {
  spdlog::debug("Workspace {} changed to state {}", id_, state_);
  auto style_context = button_.get_style_context();
  add_or_remove_class(style_context, is_active(), "active");
  add_or_remove_class(style_context, is_urgent(), "urgent");
  add_or_remove_class(style_context, is_hidden(), "hidden");
  add_or_remove_class(style_context, is_empty(), "persistent");

  if (workspace_group_.creation_delayed()) {
    return;
  }

  if (workspace_group_.active_only() && (is_active() || is_urgent())) {
    button_.show_all();
  } else if (workspace_group_.active_only() && !(is_active() || is_urgent())) {
    button_.hide();
  }
}

auto Workspace::get_icon() -> std::string {
  if (is_active()) {
    auto active_icon_it = icons_map_.find("active");
    if (active_icon_it != icons_map_.end()) {
      return active_icon_it->second;
    }
  }

  auto named_icon_it = icons_map_.find(name_);
  if (named_icon_it != icons_map_.end()) {
    return named_icon_it->second;
  }

  if (is_empty()) {
    auto persistent_icon_it = icons_map_.find("persistent");
    if (persistent_icon_it != icons_map_.end()) {
      return persistent_icon_it->second;
    }
  }

  auto default_icon_it = icons_map_.find("default");
  if (default_icon_it != icons_map_.end()) {
    return default_icon_it->second;
  }

  return name_;
}

auto Workspace::handle_clicked(GdkEventButton *bt) -> bool {
  std::string action;
  if (config_["on-click"].isString() && bt->button == 1) {
    action = config_["on-click"].asString();
  } else if (config_["on-click-middle"].isString() && bt->button == 2) {
    action = config_["on-click-middle"].asString();
  } else if (config_["on-click-right"].isString() && bt->button == 3) {
    action = config_["on-click-right"].asString();
  }

  if (action.empty())
    return true;
  else if (action == "activate") {
    zext_workspace_handle_v1_activate(workspace_handle_);
  } else if (action == "close") {
    zext_workspace_handle_v1_remove(workspace_handle_);
  } else {
    spdlog::warn("Unknown action {}", action);
  }

  workspace_group_.commit();

  return true;
}

auto Workspace::show() -> void { button_.show_all(); }
auto Workspace::hide() -> void { button_.hide(); }

auto Workspace::handle_name(const std::string &name) -> void {
  if (name_ != name) {
    workspace_group_.set_need_to_sort();
  }
  name_ = name;
  spdlog::debug("Workspace {} added to group {}", name, workspace_group_.id());

  make_persistent();
  handle_duplicate();
}

auto Workspace::make_persistent() -> void {
  auto p_workspaces = workspace_group_.persistent_workspaces();

  if (std::find(p_workspaces.begin(), p_workspaces.end(), name_) != p_workspaces.end()) {
    persistent_ = true;
  }
}

auto Workspace::handle_duplicate() -> void {
  auto duplicate =
      std::find_if(workspace_group_.workspaces().begin(), workspace_group_.workspaces().end(),
                   [this](const std::unique_ptr<Workspace> &g) {
                     return g->get_name() == name_ && g->id() != id_;
                   });
  if (duplicate != workspace_group_.workspaces().end()) {
    workspace_group_.remove_workspace(duplicate->get()->id());
  }
}

auto Workspace::handle_coordinates(const std::vector<uint32_t> &coordinates) -> void {
  if (coordinates_ != coordinates) {
    workspace_group_.set_need_to_sort();
  }
  coordinates_ = coordinates;
}
}  // namespace waybar::modules::wlr
