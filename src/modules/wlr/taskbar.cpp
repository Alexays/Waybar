#include "modules/wlr/taskbar.hpp"

#include <fmt/core.h>
#include <gdkmm/monitor.h>
#include <gio/gdesktopappinfo.h>
#include <giomm/desktopappinfo.h>
#include <gtkmm/icontheme.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <utility>

#include "gdkmm/general.h"
#include "glibmm/error.h"
#include "glibmm/fileutils.h"
#include "glibmm/refptr.h"
#include "util/format.hpp"
#include "util/gtk_icon.hpp"
#include "util/rewrite_string.hpp"
#include "util/string.hpp"

namespace waybar::modules::wlr {

/* Task class implementation */
uint32_t Task::global_id = 0;

static void tl_handle_title(void* data, struct zwlr_foreign_toplevel_handle_v1* handle,
                            const char* title) {
  return static_cast<Task*>(data)->handle_title(title);
}

static void tl_handle_app_id(void* data, struct zwlr_foreign_toplevel_handle_v1* handle,
                             const char* app_id) {
  return static_cast<Task*>(data)->handle_app_id(app_id);
}

static void tl_handle_output_enter(void* data, struct zwlr_foreign_toplevel_handle_v1* handle,
                                   struct wl_output* output) {
  return static_cast<Task*>(data)->handle_output_enter(output);
}

static void tl_handle_output_leave(void* data, struct zwlr_foreign_toplevel_handle_v1* handle,
                                   struct wl_output* output) {
  return static_cast<Task*>(data)->handle_output_leave(output);
}

static void tl_handle_state(void* data, struct zwlr_foreign_toplevel_handle_v1* handle,
                            struct wl_array* state) {
  return static_cast<Task*>(data)->handle_state(state);
}

static void tl_handle_done(void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
  return static_cast<Task*>(data)->handle_done();
}

static void tl_handle_parent(void* data, struct zwlr_foreign_toplevel_handle_v1* handle,
                             struct zwlr_foreign_toplevel_handle_v1* parent) {
  /* This is explicitly left blank */
}

static void tl_handle_closed(void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
  return static_cast<Task*>(data)->handle_closed();
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_impl = {
    .title = tl_handle_title,
    .app_id = tl_handle_app_id,
    .output_enter = tl_handle_output_enter,
    .output_leave = tl_handle_output_leave,
    .state = tl_handle_state,
    .done = tl_handle_done,
    .closed = tl_handle_closed,
    .parent = tl_handle_parent,
};

static const std::vector<Gtk::TargetEntry> target_entries = {
    Gtk::TargetEntry("WAYBAR_TOPLEVEL", Gtk::TARGET_SAME_APP, 0)};

Task::Task(const waybar::Bar& bar, const Json::Value& config, Taskbar* tbar,
           struct zwlr_foreign_toplevel_handle_v1* tl_handle, struct wl_seat* seat)
    : bar_{bar},
      config_{config},
      tbar_{tbar},
      handle_{tl_handle},
      seat_{seat},
      id_{global_id++},
      content_{bar.orientation, 0} {
  zwlr_foreign_toplevel_handle_v1_add_listener(handle_, &toplevel_handle_impl, this);

  button.set_relief(Gtk::RELIEF_NONE);

  /* When "expand" is enabled the buttons stretch to fill the taskbar and the
   * titles ellipsize to fit within the available space. This only makes sense
   * on a horizontal bar; on a vertical bar the box grows along the vertical
   * axis, so ellipsizing/forcing width_chars(1) would truncate every title to
   * "…". Keep the historical behavior (buttons sized to their content) as the
   * default and when the bar is vertical. */
  bool expand = config_["expand"].isBool() && config_["expand"].asBool();
  bool horizontal = bar.orientation == Gtk::ORIENTATION_HORIZONTAL;

  if (expand && horizontal) {
    button.set_hexpand(true);
    content_.set_hexpand(true);
    text_before_.set_ellipsize(Pango::ELLIPSIZE_END);
    text_before_.set_single_line_mode(true);
    text_before_.set_width_chars(1);
    text_before_.set_xalign(0.0);
    text_after_.set_ellipsize(Pango::ELLIPSIZE_END);
    text_after_.set_single_line_mode(true);
    text_after_.set_width_chars(1);
    text_after_.set_xalign(0.0);

    content_.pack_start(text_before_, true, true, 0);
    content_.pack_start(icon_, false, false, 0);
    content_.pack_start(text_after_, true, true, 0);
  } else {
    content_.add(text_before_);
    content_.add(icon_);
    content_.add(text_after_);
  }

  if (config_["justify"].isString()) {
    auto justify_str = config_["justify"].asString();
    if (justify_str == "left") {
      content_.set_halign(Gtk::ALIGN_START);
    } else if (justify_str == "right") {
      content_.set_halign(Gtk::ALIGN_END);
    } else if (justify_str == "center") {
      content_.set_halign(Gtk::ALIGN_CENTER);
    }
  }

  content_.show();
  button.add(content_);

  // Apply optional label truncation (ellipsize).
  if (config_["truncate"].isBool() && config_["truncate"].asBool()) {
    text_before_.set_single_line_mode(true);
    text_before_.set_ellipsize(Pango::ELLIPSIZE_END);
    text_before_.set_line_wrap(false);

    text_after_.set_single_line_mode(true);
    text_after_.set_ellipsize(Pango::ELLIPSIZE_END);
    text_after_.set_line_wrap(false);
  }

  format_before_.clear();
  format_after_.clear();

  if (config_["format"].isString()) {
    /* The user defined a format string, use it */
    auto format = config_["format"].asString();
    if (format.find("{name}") != std::string::npos) {
      with_name_ = true;
    }

    auto parts = split(format, "{icon}", 1);
    format_before_ = parts[0];
    if (parts.size() > 1) {
      with_icon_ = true;
      format_after_ = parts[1];
    }
  } else {
    /* The default is to only show the icon */
    with_icon_ = true;
  }

  if (app_id_.empty()) {
    handle_app_id("unknown");
  }

  /* Strip spaces at the beginning and end of the format strings */
  format_tooltip_.clear();
  if (!config_["tooltip"].isBool() || config_["tooltip"].asBool()) {
    if (config_["tooltip-format"].isString())
      format_tooltip_ = config_["tooltip-format"].asString();
    else
      format_tooltip_ = "{title}";
  }

  /* Handle click events if configured */
  if (config_["on-click"].isString() || config_["on-click-middle"].isString() ||
      config_["on-click-right"].isString()) {
  }

  button.add_events(Gdk::BUTTON_PRESS_MASK);
  button.signal_button_release_event().connect(sigc::mem_fun(*this, &Task::handle_clicked), false);

  button.signal_motion_notify_event().connect(sigc::mem_fun(*this, &Task::handle_motion_notify),
                                              false);

  button.drag_source_set(target_entries, Gdk::BUTTON1_MASK, Gdk::ACTION_MOVE);
  button.drag_dest_set(target_entries, Gtk::DEST_DEFAULT_ALL, Gdk::ACTION_MOVE);

  button.signal_drag_data_get().connect(sigc::mem_fun(*this, &Task::handle_drag_data_get), false);
  button.signal_drag_data_received().connect(sigc::mem_fun(*this, &Task::handle_drag_data_received),
                                             false);
}

Task::~Task() {
  if (handle_) {
    zwlr_foreign_toplevel_handle_v1_destroy(handle_);
    handle_ = nullptr;
  }
  if (button_visible_) {
    tbar_->remove_button(button);
    button_visible_ = false;
  }
}

std::string Task::repr() const {
  std::stringstream ss;
  ss << "Task (" << id_ << ") " << title_ << " [" << app_id_ << "] <" << (active() ? "A" : "a")
     << (maximized() ? "M" : "m") << (minimized() ? "I" : "i") << (fullscreen() ? "F" : "f") << ">";

  return ss.str();
}

std::string Task::state_string(bool shortened) const {
  std::stringstream ss;
  if (shortened)
    ss << (minimized() ? "m" : "") << (maximized() ? "M" : "") << (active() ? "A" : "")
       << (fullscreen() ? "F" : "");
  else
    ss << (minimized() ? "minimized " : "") << (maximized() ? "maximized " : "")
       << (active() ? "active " : "") << (fullscreen() ? "fullscreen " : "");

  std::string res = ss.str();
  if (shortened || res.empty())
    return res;
  else
    return res.substr(0, res.size() - 1);
}

void Task::handle_title(const char* title) {
  if (title_.empty()) {
    spdlog::debug(fmt::format("Task ({}) setting title to {}", id_, title_));
  } else {
    spdlog::debug(fmt::format("Task ({}) overwriting title '{}' with '{}'", id_, title_, title));
  }
  title_ = title;
  hide_if_ignored();
  hide_if_duplicate();

  if ((!with_icon_ && !with_name_) || app_info_) {
    return;
  }

  app_info_ = IconLoader::get_app_info_from_app_id_list(title_);
  name_ = app_info_ ? app_info_->get_display_name() : title;

  if (!with_icon_) {
    return;
  }

  int icon_size = config_["icon-size"].isInt() ? config_["icon-size"].asInt() : 16;
  if (tbar_->icon_loader().image_load_icon(icon_, app_info_, icon_size))
    icon_.show();
  else
    spdlog::debug("Couldn't find icon for {}", title_);
}

void Task::set_minimize_hint() {
  zwlr_foreign_toplevel_handle_v1_set_rectangle(handle_, bar_.surface, minimize_hint.x,
                                                minimize_hint.y, minimize_hint.w, minimize_hint.h);
}

void Task::hide_if_duplicate() {
  const auto& squash_list = tbar_->squash_list();
  bool contains_app =
      squash_list.contains("*") || squash_list.contains(title_) || squash_list.contains(app_id_);

  // Squashes if the app is in the squash list and more than 1 instance is open
  if (contains_app && (tbar_->task_id_count(app_id_) > 1 || tbar_->task_title_count(title_) > 1)) {
    squashed_ = true;
    hide_button();
  }

  if (!squashed_ && !ignored_ && (tbar_->all_outputs() || on_bar_output_)) {
    show_button();
  }
}

void Task::hide_if_ignored() {
  if (tbar_->ignore_list().count(app_id_) || tbar_->ignore_list().count(title_)) {
    ignored_ = true;
    hide_button();
    return;
  }

  if (ignored_) {
    /* The app_id/title changed to a value that is no longer ignored */
    ignored_ = false;
    if (!squashed_ && (tbar_->all_outputs() || on_bar_output_)) {
      show_button();
    }
  }
}

void Task::handle_app_id(const char* app_id) {
  if (app_id_.empty()) {
    spdlog::debug(fmt::format("Task ({}) setting app_id to {}", id_, app_id));
  } else {
    spdlog::debug(fmt::format("Task ({}) overwriting app_id '{}' with '{}'", id_, app_id_, app_id));
  }
  app_id_ = app_id;
  hide_if_ignored();
  hide_if_duplicate();

  auto ids_replace_map = tbar_->app_ids_replace_map();
  if (ids_replace_map.count(app_id_)) {
    auto replaced_id = ids_replace_map[app_id_];
    spdlog::debug(
        fmt::format("Task ({}) [{}] app_id was replaced with {}", id_, app_id_, replaced_id));
    app_id_ = replaced_id;
  }

  if (!with_icon_ && !with_name_) {
    return;
  }

  app_info_ = IconLoader::get_app_info_from_app_id_list(app_id_);
  name_ = app_info_ ? app_info_->get_display_name() : app_id;

  if (!with_icon_) {
    return;
  }

  int icon_size = config_["icon-size"].isInt() ? config_["icon-size"].asInt() : 16;
  if (tbar_->icon_loader().image_load_icon(icon_, app_info_, icon_size))
    icon_.show();
  else
    spdlog::debug("Couldn't find icon for {}", app_id_);
}

void Task::on_button_size_allocated(Gtk::Allocation& alloc) {
  gtk_widget_translate_coordinates(GTK_WIDGET(button.gobj()), GTK_WIDGET(bar_.window.gobj()), 0, 0,
                                   &minimize_hint.x, &minimize_hint.y);
  minimize_hint.w = button.get_width();
  minimize_hint.h = button.get_height();
}

void Task::handle_output_enter(struct wl_output* output) {
  spdlog::debug("{} entered output {}", repr(), (void*)output);

  if (tbar_->show_output(output)) {
    on_bar_output_ = true;
  }

  if (ignored_) {
    spdlog::debug("{} is ignored", repr());
    return;
  }
  if (squashed_) {
    spdlog::debug("{} was squashed", repr());
    return;
  }

  if (tbar_->all_outputs() || on_bar_output_) {
    /* The task entered the output of the current bar, make the button visible */
    show_button();
  }
}

void Task::handle_output_leave(struct wl_output* output) {
  spdlog::debug("{} left output {}", repr(), (void*)output);

  if (tbar_->show_output(output)) {
    on_bar_output_ = false;
  }

  if (!tbar_->all_outputs() && !on_bar_output_) {
    /* The task left the output of the current bar, make the button invisible */
    hide_button();
  }
}

void Task::show_button() {
  if (button_visible_) {
    return;
  }
  if (!size_allocate_connected_) {
    button.signal_size_allocate().connect_notify(
        sigc::mem_fun(this, &Task::on_button_size_allocated));
    size_allocate_connected_ = true;
  }
  tbar_->add_button(button);
  if (!config_["active-only"].asBool() || active()) {
    button.show();
  }
  button_visible_ = true;
  spdlog::debug("{} now visible on {}", repr(), bar_.output->name);
  tbar_->update_bar_css_classes();
}

void Task::hide_button() {
  if (!button_visible_) {
    return;
  }
  tbar_->remove_button(button);
  button.hide();
  button_visible_ = false;
  spdlog::debug("{} now invisible on {}", repr(), bar_.output->name);
  tbar_->update_bar_css_classes();
}

void Task::handle_state(struct wl_array* state) {
  state_ = 0;
  size_t size = state->size / sizeof(uint32_t);
  for (size_t i = 0; i < size; ++i) {
    auto entry = static_cast<uint32_t*>(state->data)[i];
    if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED) state_ |= MAXIMIZED;
    if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED) state_ |= MINIMIZED;
    if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) state_ |= ACTIVE;
    if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN) state_ |= FULLSCREEN;
  }
}

void Task::handle_done() {
  spdlog::debug("{} changed", repr());

  if (state_ & MAXIMIZED) {
    button.get_style_context()->add_class("maximized");
  } else if (!(state_ & MAXIMIZED)) {
    button.get_style_context()->remove_class("maximized");
  }

  if (state_ & MINIMIZED) {
    button.get_style_context()->add_class("minimized");
  } else if (!(state_ & MINIMIZED)) {
    button.get_style_context()->remove_class("minimized");
  }

  if (state_ & ACTIVE) {
    button.get_style_context()->add_class("active");
  } else if (!(state_ & ACTIVE)) {
    button.get_style_context()->remove_class("active");
  }

  if (state_ & FULLSCREEN) {
    button.get_style_context()->add_class("fullscreen");
  } else if (!(state_ & FULLSCREEN)) {
    button.get_style_context()->remove_class("fullscreen");
  }

  if (button_visible_ && config_["active-only"].asBool()) {
    if (active()) {
      button.show();
    } else {
      button.hide();
    }
  }

  if (active()) {
    tbar_->assign_current_workspace(*this);
  }

  tbar_->update_bar_css_classes();

  if (config_["active-first"].isBool() && config_["active-first"].asBool() && active())
    tbar_->move_button(button, 0);

  tbar_->dp.emit();
}

void Task::handle_closed() {
  spdlog::debug("{} closed", repr());
  zwlr_foreign_toplevel_handle_v1_destroy(handle_);
  handle_ = nullptr;
  if (button_visible_) {
    tbar_->remove_button(button);
    button_visible_ = false;
  }

  const auto& squash_list = tbar_->squash_list();
  const bool in_squash_list =
      squash_list.contains("*") || squash_list.contains(title_) || squash_list.contains(app_id_);
  if (in_squash_list && !squashed_ &&
      (tbar_->task_id_count(app_id_) > 1 || tbar_->task_title_count(title_) > 1)) {
    // Find next squashed task with same title or id (excluding ourselves)
    auto tasks = tbar_->tasks();
    const auto it = std::ranges::find_if(tasks, [this](auto&& task) {
      return &task != this && task.squashed_ &&
             (task.app_id() == app_id_ || task.title() == title_);
    });

    if (it != tasks.end() && !(*it).ignored_) {
      Task& task = *it;
      task.squashed_ = false;
      if (tbar_->all_outputs() || task.on_bar_output_) {
        task.show_button();
      }
    }
  }

  tbar_->remove_task(id_);
}

bool Task::handle_clicked(GdkEventButton* bt) {
  /* filter out additional events for double/triple clicks */
  if (bt->type == GDK_BUTTON_PRESS) {
    /* save where the button press occurred in case it becomes a drag */
    drag_start_button = bt->button;
    drag_start_x = bt->x;
    drag_start_y = bt->y;
  }

  std::string action;
  if (config_["on-click"].isString() && bt->button == 1)
    action = config_["on-click"].asString();
  else if (config_["on-click-middle"].isString() && bt->button == 2)
    action = config_["on-click-middle"].asString();
  else if (config_["on-click-right"].isString() && bt->button == 3)
    action = config_["on-click-right"].asString();

  if (action.empty())
    return true;
  else if (action == "activate")
    activate();
  else if (action == "minimize") {
    set_minimize_hint();
    minimize(!minimized());
  } else if (action == "minimize-raise") {
    set_minimize_hint();
    if (minimized())
      minimize(false);
    else if (active())
      minimize(true);
    else
      activate();
  } else if (action == "maximize")
    maximize(!maximized());
  else if (action == "fullscreen")
    fullscreen(!fullscreen());
  else if (action == "close")
    close();
  else
    spdlog::warn("Unknown action {}", action);

  drag_start_button = -1;
  return true;
}

bool Task::handle_motion_notify(GdkEventMotion* mn) {
  if (drag_start_button == -1) return false;

  if (button.drag_check_threshold(drag_start_x, drag_start_y, mn->x, mn->y)) {
    /* start drag in addition to other assigned action */
    auto target_list = Gtk::TargetList::create(target_entries);
    auto refptr = Glib::RefPtr<Gtk::TargetList>(target_list);
    auto drag_context =
        button.drag_begin(refptr, Gdk::DragAction::ACTION_MOVE, drag_start_button, (GdkEvent*)mn);
  }

  return false;
}

void Task::handle_drag_data_get(const Glib::RefPtr<Gdk::DragContext>& context,
                                Gtk::SelectionData& selection_data, guint info, guint time) {
  spdlog::debug("drag_data_get");
  void* button_addr = (void*)&this->button;

  selection_data.set("WAYBAR_TOPLEVEL", 32, (const guchar*)&button_addr, sizeof(gpointer));
}

void Task::handle_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
                                     Gtk::SelectionData selection_data, guint info, guint time) {
  spdlog::debug("drag_data_received");
  auto* raw = selection_data.get_data();
  if (!raw || selection_data.get_length() < static_cast<int>(sizeof(gpointer))) return;
  gpointer handle = *(gpointer*)raw;
  auto dragged_button = (Gtk::Button*)handle;

  if (dragged_button == &this->button) return;

  auto parent_of_dragged = dragged_button->get_parent();
  auto parent_of_dest = this->button.get_parent();

  if (parent_of_dragged != parent_of_dest) return;

  auto box = (Gtk::Box*)parent_of_dragged;

  auto position_prop = box->child_property_position(this->button);
  auto position = position_prop.get_value();

  box->reorder_child(*dragged_button, position);
}

bool Task::operator==(const Task& o) const { return o.id_ == id_; }

bool Task::operator!=(const Task& o) const { return o.id_ != id_; }

void Task::update() {
  bool markup = config_["markup"].isBool() ? config_["markup"].asBool() : false;
  std::string title = title_;
  std::string name = name_;
  std::string app_id = app_id_;
  if (markup) {
    title = Glib::Markup::escape_text(title);
    name = Glib::Markup::escape_text(name);
    app_id = Glib::Markup::escape_text(app_id);
  }
  if (!format_before_.empty()) {
    auto txt =
        fmt::format(fmt::runtime(format_before_), fmt::arg("title", title), fmt::arg("name", name),
                    fmt::arg("app_id", app_id), fmt::arg("state", state_string()),
                    fmt::arg("short_state", state_string(true)));

    txt = waybar::util::rewriteString(txt, config_["rewrite"]);

    if (markup)
      text_before_.set_markup(txt);
    else
      text_before_.set_label(txt);
    text_before_.show();
  }
  if (!format_after_.empty()) {
    auto txt =
        fmt::format(fmt::runtime(format_after_), fmt::arg("title", title), fmt::arg("name", name),
                    fmt::arg("app_id", app_id), fmt::arg("state", state_string()),
                    fmt::arg("short_state", state_string(true)));

    txt = waybar::util::rewriteString(txt, config_["rewrite"]);

    if (markup)
      text_after_.set_markup(txt);
    else
      text_after_.set_label(txt);
    text_after_.show();
  }

  if (!format_tooltip_.empty()) {
    auto txt =
        fmt::format(fmt::runtime(format_tooltip_), fmt::arg("title", title), fmt::arg("name", name),
                    fmt::arg("app_id", app_id), fmt::arg("state", state_string()),
                    fmt::arg("short_state", state_string(true)));

    txt = waybar::util::rewriteString(txt, config_["rewrite"]);

    if (markup)
      button.set_tooltip_markup(txt);
    else
      button.set_tooltip_text(txt);
  }
}

void Task::maximize(bool set) {
  if (set)
    zwlr_foreign_toplevel_handle_v1_set_maximized(handle_);
  else
    zwlr_foreign_toplevel_handle_v1_unset_maximized(handle_);
}

void Task::minimize(bool set) {
  if (set)
    zwlr_foreign_toplevel_handle_v1_set_minimized(handle_);
  else
    zwlr_foreign_toplevel_handle_v1_unset_minimized(handle_);
}

void Task::activate() { zwlr_foreign_toplevel_handle_v1_activate(handle_, seat_); }

void Task::fullscreen(bool set) {
  if (zwlr_foreign_toplevel_handle_v1_get_version(handle_) <
      ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_SET_FULLSCREEN_SINCE_VERSION) {
    spdlog::warn("Foreign toplevel manager server does not support for set/unset fullscreen.");
    return;
  }

  if (set)
    zwlr_foreign_toplevel_handle_v1_set_fullscreen(handle_, nullptr);
  else
    zwlr_foreign_toplevel_handle_v1_unset_fullscreen(handle_);
}

void Task::close() { zwlr_foreign_toplevel_handle_v1_close(handle_); }

/* Taskbar class implementation */
static void handle_global(void* data, struct wl_registry* registry, uint32_t name,
                          const char* interface, uint32_t version) {
  if (std::strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
    static_cast<Taskbar*>(data)->register_manager(registry, name, version);
  } else if (std::strcmp(interface, ext_workspace_manager_v1_interface.name) == 0) {
    static_cast<Taskbar*>(data)->register_workspace_manager(registry, name, version);
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    static_cast<Taskbar*>(data)->register_seat(registry, name, version);
  }
}

static void handle_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
  /* Nothing to do here */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

Taskbar::Taskbar(const std::string& id, const waybar::Bar& bar, const Json::Value& config)
    : waybar::AModule(config, "taskbar", id, false, false),
      bar_(bar),
      box_{bar.orientation, 0},
      manager_{nullptr},
      workspace_manager_{nullptr},
      seat_{nullptr} {
  box_.set_name("taskbar");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  box_.get_style_context()->add_class("empty");
  event_box_.add(box_);

  // wlr/taskbar interprets on-click* config values as built-in actions, handled
  // per-task in Task::handle_clicked. Register the recognized action names so
  // AModule dispatches them via doAction() instead of also running them as shell
  // commands (issue #3284). Values that are not built-in actions are left alone
  // and still run as user shell commands.
  const auto is_builtin_action = [](const std::string& v) {
    return v == "activate" || v == "minimize" || v == "minimize-raise" || v == "maximize" ||
           v == "fullscreen" || v == "close";
  };
  for (const auto* event : {"on-click", "on-click-middle", "on-click-right"}) {
    if (config_[event].isString() && is_builtin_action(config_[event].asString())) {
      eventActionMap_.insert({event, config_[event].asString()});
    }
  }

  // Make task buttons distribute evenly across the available width.
  if (config_["homogeneous"].isBool() && config_["homogeneous"].asBool()) {
    box_.set_homogeneous(true);
    box_.set_hexpand(true);
  }

  struct wl_display* display = Client::inst()->wl_display;
  struct wl_registry* registry = wl_display_get_registry(display);

  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

  if (!manager_) {
    spdlog::error("Failed to register as toplevel manager");
    return;
  }
  if (!seat_) {
    spdlog::error("Failed to get wayland seat");
    return;
  }

  /* Get the configured icon theme if specified */
  if (config_["icon-theme"].isArray()) {
    for (auto& c : config_["icon-theme"]) {
      icon_loader_.add_custom_icon_theme(c.asString());
    }
  } else if (config_["icon-theme"].isString()) {
    icon_loader_.add_custom_icon_theme(config_["icon-theme"].asString());
  }

  // Load ignore-list
  if (config_["ignore-list"].isArray()) {
    for (auto& app_name : config_["ignore-list"]) {
      ignore_list_.emplace(app_name.asString());
    }
  }

  // Load squash-list
  if (config_["squash-list"].isArray()) {
    for (auto& app_name : config_["squash-list"]) {
      squash_list_.emplace(app_name.asString());
    }
  }

  // Load app_id remappings
  if (config_["app_ids-mapping"].isObject()) {
    const Json::Value& mapping = config_["app_ids-mapping"];
    const std::vector<std::string> app_ids = config_["app_ids-mapping"].getMemberNames();
    for (auto& app_id : app_ids) {
      app_ids_replace_map_.emplace(app_id, mapping[app_id].asString());
    }
  }

  for (auto& t : tasks_) {
    t->handle_app_id(t->app_id().c_str());
  }
}

Taskbar::~Taskbar() {
  for (auto& workspace : workspaces_) {
    ext_workspace_handle_v1_destroy(workspace->handle);
  }
  workspaces_.clear();
  for (auto* group : workspace_groups_) {
    ext_workspace_group_handle_v1_destroy(group);
  }
  workspace_groups_.clear();

  if (workspace_manager_) {
    struct wl_display* display = Client::inst()->wl_display;
    ext_workspace_manager_v1_stop(workspace_manager_);
    wl_display_roundtrip(display);

    if (workspace_manager_) {
      spdlog::warn("Workspace manager destroyed before .finished event");
      ext_workspace_manager_v1_destroy(workspace_manager_);
      workspace_manager_ = nullptr;
    }
  }

  if (manager_) {
    struct wl_display* display = Client::inst()->wl_display;
    /*
     * Send `stop` request and wait for one roundtrip.
     * This is not quite correct as the protocol encourages us to wait for the .finished event,
     * but it should work with wlroots foreign toplevel manager implementation.
     */
    zwlr_foreign_toplevel_manager_v1_stop(manager_);
    wl_display_roundtrip(display);

    if (manager_) {
      spdlog::warn("Foreign toplevel manager destroyed before .finished event");
      zwlr_foreign_toplevel_manager_v1_destroy(manager_);
      manager_ = nullptr;
    }
  }

  if (config_["bar-css-states"].asBool()) {
    set_bar_css_class("toplevel-active", false);
    set_bar_css_class("toplevel-maximized", false);
    set_bar_css_class("toplevel-minimized", false);
    set_bar_css_class("toplevel-fullscreen", false);
  }
}

void Taskbar::update() {
  for (auto& t : tasks_) {
    t->update();
  }

  if (config_["sort-by-app-id"].asBool()) {
    std::stable_sort(tasks_.begin(), tasks_.end(),
                     [](const std::unique_ptr<Task>& a, const std::unique_ptr<Task>& b) {
                       return a->app_id() < b->app_id();
                     });

    for (unsigned long i = 0; i < tasks_.size(); i++) {
      move_button(tasks_[i]->button, i);
    }
  }

  AModule::update();
}

static void tm_handle_toplevel(void* data, struct zwlr_foreign_toplevel_manager_v1* manager,
                               struct zwlr_foreign_toplevel_handle_v1* tl_handle) {
  return static_cast<Taskbar*>(data)->handle_toplevel_create(tl_handle);
}

static void tm_handle_finished(void* data, struct zwlr_foreign_toplevel_manager_v1* manager) {
  return static_cast<Taskbar*>(data)->handle_finished();
}

static const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_impl = {
    .toplevel = tm_handle_toplevel,
    .finished = tm_handle_finished,
};

static void workspace_handle_id(void*, struct ext_workspace_handle_v1*, const char*) {}
static void workspace_handle_name(void*, struct ext_workspace_handle_v1*, const char*) {}
static void workspace_handle_coordinates(void*, struct ext_workspace_handle_v1*, struct wl_array*) {
}

static void workspace_handle_state(void* data, struct ext_workspace_handle_v1*, uint32_t state) {
  static_cast<Taskbar::WorkspaceState*>(data)->state = state;
}

static void workspace_handle_capabilities(void*, struct ext_workspace_handle_v1*, uint32_t) {}

static void workspace_handle_removed(void* data, struct ext_workspace_handle_v1* handle) {
  static_cast<Taskbar::WorkspaceState*>(data)->taskbar->handle_workspace_removed(handle);
}

static const struct ext_workspace_handle_v1_listener workspace_handle_impl = {
    .id = workspace_handle_id,
    .name = workspace_handle_name,
    .coordinates = workspace_handle_coordinates,
    .state = workspace_handle_state,
    .capabilities = workspace_handle_capabilities,
    .removed = workspace_handle_removed,
};

static void workspace_group_handle_capabilities(void*, struct ext_workspace_group_handle_v1*,
                                                uint32_t) {}
static void workspace_group_handle_output_enter(void*, struct ext_workspace_group_handle_v1*,
                                                struct wl_output*) {}
static void workspace_group_handle_output_leave(void*, struct ext_workspace_group_handle_v1*,
                                                struct wl_output*) {}
static void workspace_group_handle_workspace_enter(void*, struct ext_workspace_group_handle_v1*,
                                                   struct ext_workspace_handle_v1*) {}
static void workspace_group_handle_workspace_leave(void*, struct ext_workspace_group_handle_v1*,
                                                   struct ext_workspace_handle_v1*) {}

static void workspace_group_handle_removed(void* data,
                                           struct ext_workspace_group_handle_v1* group) {
  static_cast<Taskbar*>(data)->handle_workspace_group_removed(group);
}

static const struct ext_workspace_group_handle_v1_listener workspace_group_impl = {
    .capabilities = workspace_group_handle_capabilities,
    .output_enter = workspace_group_handle_output_enter,
    .output_leave = workspace_group_handle_output_leave,
    .workspace_enter = workspace_group_handle_workspace_enter,
    .workspace_leave = workspace_group_handle_workspace_leave,
    .removed = workspace_group_handle_removed,
};

static void workspace_manager_handle_group(void* data, struct ext_workspace_manager_v1*,
                                           struct ext_workspace_group_handle_v1* group) {
  static_cast<Taskbar*>(data)->handle_workspace_group_create(group);
}

static void workspace_manager_handle_workspace(void* data, struct ext_workspace_manager_v1*,
                                               struct ext_workspace_handle_v1* workspace) {
  static_cast<Taskbar*>(data)->handle_workspace_create(workspace);
}

static void workspace_manager_handle_done(void* data, struct ext_workspace_manager_v1*) {
  static_cast<Taskbar*>(data)->handle_workspace_done();
}

static void workspace_manager_handle_finished(void* data, struct ext_workspace_manager_v1*) {
  static_cast<Taskbar*>(data)->handle_workspace_finished();
}

static const struct ext_workspace_manager_v1_listener workspace_manager_impl = {
    .workspace_group = workspace_manager_handle_group,
    .workspace = workspace_manager_handle_workspace,
    .done = workspace_manager_handle_done,
    .finished = workspace_manager_handle_finished,
};

void Taskbar::register_manager(struct wl_registry* registry, uint32_t name, uint32_t version) {
  if (manager_) {
    spdlog::warn("Register foreign toplevel manager again although already existing!");
    return;
  }
  if (version < ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_SET_FULLSCREEN_SINCE_VERSION) {
    spdlog::warn(
        "Foreign toplevel manager server does not have the appropriate version."
        " To be able to use all features, you need at least version 2, but server is version {}",
        version);
  }

  // limit version to a highest supported by the client protocol file
  version = std::min<uint32_t>(version, zwlr_foreign_toplevel_manager_v1_interface.version);

  manager_ = static_cast<struct zwlr_foreign_toplevel_manager_v1*>(
      wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, version));

  if (manager_)
    zwlr_foreign_toplevel_manager_v1_add_listener(manager_, &toplevel_manager_impl, this);
  else
    spdlog::debug("Failed to register manager");
}

void Taskbar::register_workspace_manager(struct wl_registry* registry, uint32_t name,
                                         uint32_t version) {
  if (workspace_manager_) {
    return;
  }

  version = std::min<uint32_t>(version, ext_workspace_manager_v1_interface.version);
  workspace_manager_ = static_cast<struct ext_workspace_manager_v1*>(
      wl_registry_bind(registry, name, &ext_workspace_manager_v1_interface, version));
  ext_workspace_manager_v1_add_listener(workspace_manager_, &workspace_manager_impl, this);
}

void Taskbar::register_seat(struct wl_registry* registry, uint32_t name, uint32_t version) {
  if (seat_) {
    spdlog::warn("Register seat again although already existing!");
    return;
  }
  version = std::min<uint32_t>(version, wl_seat_interface.version);

  seat_ = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, version));
}

void Taskbar::handle_toplevel_create(struct zwlr_foreign_toplevel_handle_v1* tl_handle) {
  tasks_.push_back(std::make_unique<Task>(bar_, config_, this, tl_handle, seat_));
}

void Taskbar::handle_finished() {
  zwlr_foreign_toplevel_manager_v1_destroy(manager_);
  manager_ = nullptr;
}

void Taskbar::handle_workspace_group_create(struct ext_workspace_group_handle_v1* handle) {
  ext_workspace_group_handle_v1_add_listener(handle, &workspace_group_impl, this);
  workspace_groups_.push_back(handle);
}

void Taskbar::handle_workspace_group_removed(struct ext_workspace_group_handle_v1* handle) {
  const auto group = std::find(workspace_groups_.begin(), workspace_groups_.end(), handle);
  if (group != workspace_groups_.end()) {
    ext_workspace_group_handle_v1_destroy(*group);
    workspace_groups_.erase(group);
  }
}

void Taskbar::handle_workspace_create(struct ext_workspace_handle_v1* handle) {
  auto workspace = std::make_unique<WorkspaceState>(WorkspaceState{this, handle});
  ext_workspace_handle_v1_add_listener(handle, &workspace_handle_impl, workspace.get());
  workspaces_.push_back(std::move(workspace));
}

void Taskbar::handle_workspace_done() {
  const auto active_workspace =
      std::find_if(workspaces_.begin(), workspaces_.end(), [](const auto& workspace) {
        return workspace->state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;
      });
  current_workspace_ =
      active_workspace == workspaces_.end() ? nullptr : (*active_workspace)->handle;
  if (current_workspace_) {
    const auto active_task =
        std::find_if(tasks_.begin(), tasks_.end(), [](const auto& task) { return task->active(); });
    if (active_task != tasks_.end()) {
      (*active_task)->set_workspace(current_workspace_);
    }
  }
  update_bar_css_classes();
}

void Taskbar::handle_workspace_finished() { workspace_manager_ = nullptr; }

void Taskbar::handle_workspace_removed(struct ext_workspace_handle_v1* handle) {
  if (current_workspace_ == handle) {
    current_workspace_ = nullptr;
  }
  for (auto& task : tasks_) {
    if (task->workspace() == handle) {
      task->set_workspace(nullptr);
    }
  }

  const auto workspace =
      std::find_if(workspaces_.begin(), workspaces_.end(),
                   [handle](const auto& workspace) { return workspace->handle == handle; });
  if (workspace != workspaces_.end()) {
    ext_workspace_handle_v1_destroy((*workspace)->handle);
    workspaces_.erase(workspace);
  }
  update_bar_css_classes();
}

void Taskbar::add_button(Gtk::Button& bt) {
  /* When "homogeneous" is enabled, let every child expand and fill so the buttons
   * divide the available width equally. Otherwise, only let the buttons expand to
   * fill the taskbar when "expand" is enabled and the bar is horizontal (see the
   * Task constructor for details). */
  const bool homogeneous = config_["homogeneous"].isBool() && config_["homogeneous"].asBool();
  bool expand = config_["expand"].isBool() && config_["expand"].asBool();
  bool horizontal = bar_.orientation == Gtk::ORIENTATION_HORIZONTAL;
  if (homogeneous) {
    box_.pack_start(bt, true, true);
    bt.set_hexpand(true);
    bt.set_halign(Gtk::ALIGN_FILL);
  } else if (expand && horizontal) {
    box_.pack_start(bt, true, true);
  } else {
    box_.pack_start(bt, false, false);
  }
  box_.get_style_context()->remove_class("empty");
}

void Taskbar::move_button(Gtk::Button& bt, int pos) { box_.reorder_child(bt, pos); }

void Taskbar::remove_button(Gtk::Button& bt) {
  box_.remove(bt);
  if (box_.get_children().empty()) {
    box_.get_style_context()->add_class("empty");
  }
}

void Taskbar::remove_task(uint32_t id) {
  auto it = std::find_if(std::begin(tasks_), std::end(tasks_),
                         [id](const TaskPtr& p) { return p->id() == id; });

  if (it == std::end(tasks_)) {
    spdlog::warn("Can't find task with id {}", id);
    return;
  }

  tasks_.erase(it);
  update_bar_css_classes();
}

void Taskbar::assign_current_workspace(Task& task) {
  if (current_workspace_) {
    task.set_workspace(current_workspace_);
  }
}

void Taskbar::update_bar_css_classes() {
  if (!config_["bar-css-states"].asBool()) {
    return;
  }

  const auto active_task = std::find_if(tasks_.begin(), tasks_.end(), [](const TaskPtr& task) {
    return task->visible() && task->active();
  });

  const bool has_active_task = active_task != tasks_.end();
  const auto on_current_workspace = [this](const TaskPtr& task) {
    if (!current_workspace_) {
      return task->active();
    }
    return task->workspace() == current_workspace_;
  };
  const bool has_maximized_task =
      std::any_of(tasks_.begin(), tasks_.end(), [&on_current_workspace](const TaskPtr& task) {
        return task->visible() && !task->minimized() && on_current_workspace(task) &&
               task->maximized();
      });
  const bool has_fullscreen_task =
      std::any_of(tasks_.begin(), tasks_.end(), [&on_current_workspace](const TaskPtr& task) {
        return task->visible() && !task->minimized() && on_current_workspace(task) &&
               task->fullscreen();
      });
  set_bar_css_class("toplevel-active", has_active_task);
  set_bar_css_class("toplevel-maximized", has_maximized_task);
  set_bar_css_class("toplevel-minimized", has_active_task && (*active_task)->minimized());
  set_bar_css_class("toplevel-fullscreen", has_fullscreen_task);
}

void Taskbar::set_bar_css_class(const std::string& class_name, bool enabled) {
  const auto style = bar_.window.get_style_context();
  if (enabled && !style->has_class(class_name)) {
    spdlog::trace("Adding bar class: {}", class_name);
    style->add_class(class_name);
  } else if (!enabled && style->has_class(class_name)) {
    spdlog::trace("Removing bar class: {}", class_name);
    style->remove_class(class_name);
  }
}

bool Taskbar::show_output(struct wl_output* output) const {
  return output == gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
}

bool Taskbar::all_outputs() const {
  return config_["all-outputs"].isBool() && config_["all-outputs"].asBool();
}

const IconLoader& Taskbar::icon_loader() const { return icon_loader_; }

const std::unordered_set<std::string>& Taskbar::ignore_list() const { return ignore_list_; }

const std::unordered_set<std::string>& Taskbar::squash_list() const { return squash_list_; }

const std::map<std::string, std::string>& Taskbar::app_ids_replace_map() const {
  return app_ids_replace_map_;
}

std::size_t Taskbar::task_id_count(std::string_view id) const {
  return std::ranges::count_if(tasks_, [=](auto&& task) { return id == task->app_id(); });
}

std::size_t Taskbar::task_title_count(std::string_view title) const {
  return std::ranges::count_if(tasks_, [=](auto&& task) { return title == task->title(); });
}

} /* namespace waybar::modules::wlr */
