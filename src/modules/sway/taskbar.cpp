#include "modules/sway/taskbar.hpp"

#include <fmt/core.h>
#include <giomm/desktopappinfo.h>
#include <glibmm/markup.h>
#include <gtkmm/targetentry.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>
#include <exception>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "bar.hpp"
#include "util/rewrite_string.hpp"
#include "util/string.hpp"

namespace waybar::modules::sway {

namespace {

const std::vector<Gtk::TargetEntry> kTargetEntries = {
    Gtk::TargetEntry("WAYBAR_TOPLEVEL", Gtk::TARGET_SAME_APP, 0)};

// Returns {app_id, app_class} for a view. Native Wayland views carry an
// `app_id`; XWayland views instead carry the X11 `instance` and `class` hints.
// The instance is used as the displayed app_id (as sway/window does), but the
// class is carried along because it is the identifier icon lookup actually
// needs: X11 Firefox reports instance "Navigator" and class "firefox".
std::pair<std::string, std::string> resolve_app_id(
    const Json::Value& node, const std::map<std::string, std::string>& replace_map) {
  std::string app_id;
  std::string app_class;
  if (node["window_properties"]["class"].isString()) {
    app_class = node["window_properties"]["class"].asString();
  }
  if (node["app_id"].isString()) {
    app_id = node["app_id"].asString();
  } else if (node["window_properties"]["instance"].isString()) {
    app_id = node["window_properties"]["instance"].asString();
  } else {
    app_id = app_class;
  }

  const auto replace = [&replace_map](std::string& value) {
    const auto it = replace_map.find(value);
    if (it != replace_map.end()) {
      value = it->second;
    }
  };
  replace(app_id);
  replace(app_class);
  return {app_id, app_class};
}

bool is_leaf_view(const Json::Value& node) {
  const auto type = node["type"].asString();
  return (type == "con" || type == "floating_con") && node["nodes"].empty() &&
         node["floating_nodes"].empty();
}

// Recursively walks the sway node tree (as returned by GET_TREE), collecting
// every leaf view into `windows` and tracking which workspace is currently
// globally focused. `output`/`workspace`/`workspace_visible` describe the
// nearest ancestor output/workspace at each point in the recursion. A workspace
// is "visible" when its name matches its output's `current_workspace` — GET_TREE
// workspace nodes have no `visible` field (unlike GET_WORKSPACES).
void walk_tree(const Json::Value& node, std::string output, std::string output_current_ws,
               std::string workspace, bool workspace_visible, bool all_outputs,
               const std::string& bar_output, const std::map<std::string, std::string>& replace_map,
               std::vector<TaskInfo>& windows, std::string& focused_workspace) {
  const auto type = node["type"].asString();
  if (type == "output") {
    output = node["name"].asString();
    output_current_ws =
        node["current_workspace"].isString() ? node["current_workspace"].asString() : "";
  } else if (type == "workspace") {
    workspace = node["name"].asString();
    workspace_visible = workspace == output_current_ws;
    if (node["focused"].asBool()) {
      focused_workspace = workspace;
    }
  }

  if (is_leaf_view(node)) {
    if (node["focused"].asBool()) {
      focused_workspace = workspace;
    }

    if (all_outputs || (output == bar_output && workspace_visible)) {
      TaskInfo info;
      info.id = node["id"].asInt64();
      info.title = node["name"].isString() ? node["name"].asString() : "";
      std::tie(info.app_id, info.app_class) = resolve_app_id(node, replace_map);
      info.active = node["focused"].asBool();
      info.fullscreen = node["fullscreen_mode"].asInt() != 0;
      info.urgent = node["urgent"].asBool();
      info.workspace = workspace;
      windows.push_back(std::move(info));
    }
    return;
  }

  for (const auto& child : node["nodes"]) {
    walk_tree(child, output, output_current_ws, workspace, workspace_visible, all_outputs,
              bar_output, replace_map, windows, focused_workspace);
  }
  for (const auto& child : node["floating_nodes"]) {
    walk_tree(child, output, output_current_ws, workspace, workspace_visible, all_outputs,
              bar_output, replace_map, windows, focused_workspace);
  }
}

}  // namespace

/* Task class implementation */

Task::Task(const waybar::Bar& bar, const Json::Value& config, Taskbar* tbar, const TaskInfo& info)
    : config_{config}, tbar_{tbar}, id_{info.id}, content_{bar.orientation, 0} {
  button.set_relief(Gtk::RELIEF_NONE);

  markup_ = config_["markup"].isBool() && config_["markup"].asBool();
  active_only_ = config_["active-only"].isBool() && config_["active-only"].asBool();
  icon_size_ = config_["icon-size"].isInt() ? config_["icon-size"].asInt() : 16;

  /* When "expand" is enabled the buttons stretch to fill the taskbar and the
   * titles ellipsize to fit within the available space. This only makes sense
   * on a horizontal bar; see wlr/taskbar for the rationale. */
  const bool expand = config_["expand"].isBool() && config_["expand"].asBool();
  const bool horizontal = bar.orientation == Gtk::ORIENTATION_HORIZONTAL;
  const bool truncate = config_["truncate"].isBool() && config_["truncate"].asBool();

  /* Both "truncate" and the "expand" layout want single-line, ellipsized
   * labels; configure that once for either. */
  if (truncate || (expand && horizontal)) {
    for (auto* label : {&text_before_, &text_after_}) {
      label->set_single_line_mode(true);
      label->set_ellipsize(Pango::ELLIPSIZE_END);
      label->set_line_wrap(false);
    }
  }

  if (expand && horizontal) {
    button.set_hexpand(true);
    content_.set_hexpand(true);
    text_before_.set_width_chars(1);
    text_before_.set_xalign(0.0);
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

  if (config_["format"].isString()) {
    /* The user defined a format string, use it */
    auto format = config_["format"].asString();
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

  if (!config_["tooltip"].isBool() || config_["tooltip"].asBool()) {
    if (config_["tooltip-format"].isString())
      format_tooltip_ = config_["tooltip-format"].asString();
    else
      format_tooltip_ = "{title}";
  }

  button.signal_button_release_event().connect(sigc::mem_fun(*this, &Task::handleClicked), false);

  /* Reordering is handled by GTK's automatic drag handling: drag_source_set()
   * starts the drag, the button's con_id travels in the selection data. */
  button.drag_source_set(kTargetEntries, Gdk::BUTTON1_MASK, Gdk::ACTION_MOVE);
  button.drag_dest_set(kTargetEntries, Gtk::DEST_DEFAULT_ALL, Gdk::ACTION_MOVE);

  button.signal_drag_data_get().connect(sigc::mem_fun(*this, &Task::handleDragDataGet), false);
  button.signal_drag_data_received().connect(sigc::mem_fun(*this, &Task::handleDragDataReceived),
                                             false);

  setData(info);
}

Task::~Task() {
  if (button_visible_) {
    tbar_->removeButton(button);
    button_visible_ = false;
  }
}

std::string Task::stateString(bool shortened) const {
  std::stringstream ss;
  if (shortened) {
    ss << (active_ ? "A" : "") << (fullscreen_ ? "F" : "") << (urgent_ ? "U" : "");
  } else {
    ss << (active_ ? "active " : "") << (fullscreen_ ? "fullscreen " : "")
       << (urgent_ ? "urgent " : "");
  }

  std::string res = ss.str();
  if (shortened || res.empty()) {
    return res;
  }
  return res.substr(0, res.size() - 1);
}

void Task::applyVisibility(bool ignored, bool squashed) {
  if (ignored || squashed) {
    hideButton();
  } else {
    showButton();
  }
}

void Task::showButton() {
  if (button_visible_) {
    return;
  }
  tbar_->addButton(button);
  button_visible_ = true;

  if (!active_only_ || active_) {
    button.show();
  }
}

void Task::hideButton() {
  if (!button_visible_) {
    return;
  }
  tbar_->removeButton(button);
  button.hide();
  button_visible_ = false;
}

void Task::setData(const TaskInfo& info) {
  /* The desktop file and the icon are keyed on the app_id alone, so they are
   * only re-resolved when the app_id actually changes. Titles change on every
   * page navigation or shell command; re-scanning desktop files for those
   * would cost a Gio/IconTheme lookup per keystroke. */
  const bool identity_changed =
      !app_info_resolved_ || app_id_ != info.app_id || app_class_ != info.app_class;

  title_ = info.title;
  app_id_ = info.app_id;
  app_class_ = info.app_class;
  active_ = info.active;
  fullscreen_ = info.fullscreen;
  urgent_ = info.urgent;
  workspace_ = info.workspace;

  if (!identity_changed) {
    return;
  }
  app_info_resolved_ = true;

  app_info_ = IconLoader::get_app_info_from_app_id_list(app_id_);
  if (!app_info_ && !app_class_.empty() && app_class_ != app_id_) {
    app_info_ = IconLoader::get_app_info_from_app_id_list(app_class_);
  }
  name_ = app_info_ ? app_info_->get_display_name() : app_id_;

  if (!with_icon_) {
    return;
  }

  if (tbar_->iconLoader().image_load_icon(icon_, app_info_, icon_size_)) {
    icon_.show();
  } else {
    spdlog::debug("Couldn't find icon for {}", app_id_);
  }
}

bool Task::handleClicked(GdkEventButton* bt) {
  std::string action;
  if (config_["on-click"].isString() && bt->button == 1)
    action = config_["on-click"].asString();
  else if (config_["on-click-middle"].isString() && bt->button == 2)
    action = config_["on-click-middle"].asString();
  else if (config_["on-click-right"].isString() && bt->button == 3)
    action = config_["on-click-right"].asString();

  if (action.empty()) {
    return true;
  }

  try {
    if (action == "activate") {
      tbar_->ipc().sendCmd(IPC_COMMAND, fmt::format("[con_id={}] focus", id_));
    } else if (action == "close") {
      tbar_->ipc().sendCmd(IPC_COMMAND, fmt::format("[con_id={}] kill", id_));
    } else if (action == "fullscreen") {
      tbar_->ipc().sendCmd(IPC_COMMAND, fmt::format("[con_id={}] fullscreen toggle", id_));
    } else if (action == "minimize" || action == "minimize-raise" || action == "maximize") {
      spdlog::warn("{} is not supported on sway", action);
    } else {
      spdlog::warn("Unknown action {}", action);
    }
  } catch (const std::exception& e) {
    spdlog::error("Taskbar: {}", e.what());
  }

  return true;
}

void Task::handleDragDataGet(const Glib::RefPtr<Gdk::DragContext>& context,
                             Gtk::SelectionData& selection_data, guint info, guint time) {
  spdlog::debug("drag_data_get");
  /* Send the con_id rather than a pointer to this Task's button: the task may
   * be destroyed between drag-begin and drop (its window closed, or a tree
   * refresh pruned it), and a stale id is a harmless lookup miss. */
  selection_data.set("WAYBAR_TOPLEVEL", std::to_string(id_));
}

void Task::handleDragDataReceived(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
                                  Gtk::SelectionData selection_data, guint info, guint time) {
  spdlog::debug("drag_data_received");
  const auto payload = selection_data.get_data_as_string();
  int64_t dragged_id = -1;
  const auto* first = payload.data();
  const auto* last = payload.data() + payload.size();
  if (std::from_chars(first, last, dragged_id).ec != std::errc{}) {
    return;
  }

  tbar_->reorderTask(dragged_id, id_);
}

std::string Task::formatText(const std::string& format, const std::string& title,
                             const std::string& name, const std::string& app_id) {
  try {
    return fmt::format(fmt::runtime(format), fmt::arg("title", title), fmt::arg("name", name),
                       fmt::arg("app_id", app_id), fmt::arg("state", stateString()),
                       fmt::arg("short_state", stateString(true)));
  } catch (const std::exception& e) {
    /* A malformed format (e.g. an unknown {placeholder}) makes fmt throw a
     * fmt::format_error. update() runs from a Glib::Dispatcher callback, so
     * letting it escape would terminate waybar on every update; warn once
     * instead. */
    if (!format_warned_) {
      spdlog::warn("sway/taskbar: invalid format '{}': {}", format, e.what());
      format_warned_ = true;
    }
    return {};
  }
}

void Task::update() {
  std::string title = title_;
  std::string name = name_;
  std::string app_id = app_id_;
  if (markup_) {
    title = Glib::Markup::escape_text(title);
    name = Glib::Markup::escape_text(name);
    app_id = Glib::Markup::escape_text(app_id);
  }
  if (!format_before_.empty()) {
    auto txt = formatText(format_before_, title, name, app_id);
    txt = waybar::util::rewriteString(txt, config_["rewrite"]);

    if (markup_)
      text_before_.set_markup(txt);
    else
      text_before_.set_label(txt);
    text_before_.show();
  }
  if (!format_after_.empty()) {
    auto txt = formatText(format_after_, title, name, app_id);
    txt = waybar::util::rewriteString(txt, config_["rewrite"]);

    if (markup_)
      text_after_.set_markup(txt);
    else
      text_after_.set_label(txt);
    text_after_.show();
  }

  if (!format_tooltip_.empty()) {
    auto txt = formatText(format_tooltip_, title, name, app_id);
    txt = waybar::util::rewriteString(txt, config_["rewrite"]);

    if (markup_)
      button.set_tooltip_markup(txt);
    else
      button.set_tooltip_text(txt);
  }

  auto style = button.get_style_context();
  if (active_)
    style->add_class("active");
  else
    style->remove_class("active");

  if (fullscreen_)
    style->add_class("fullscreen");
  else
    style->remove_class("fullscreen");

  if (urgent_)
    style->add_class("urgent");
  else
    style->remove_class("urgent");

  if (button_visible_ && active_only_) {
    if (active_)
      button.show();
    else
      button.hide();
  }
}

/* Taskbar class implementation */

Taskbar::Taskbar(const std::string& id, const waybar::Bar& bar, const Json::Value& config)
    : waybar::AModule(config, "taskbar", id, false, false), bar_(bar), box_{bar.orientation, 0} {
  box_.set_name("taskbar");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  box_.get_style_context()->add_class("empty");
  event_box_.add(box_);

  bar_css_states_ = config_["bar-css-states"].isBool() && config_["bar-css-states"].asBool();
  sort_by_app_id_ = config_["sort-by-app-id"].isBool() && config_["sort-by-app-id"].asBool();
  active_first_ = config_["active-first"].isBool() && config_["active-first"].asBool();
  homogeneous_ = config_["homogeneous"].isBool() && config_["homogeneous"].asBool();
  expand_ = config_["expand"].isBool() && config_["expand"].asBool();

  // sway/taskbar interprets on-click* config values as built-in actions, handled
  // per-task in Task::handleClicked. Register the recognized action names so
  // AModule dispatches them via doAction() instead of also running them as shell
  // commands (mirrors wlr/taskbar, see issue #3284).
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
  if (homogeneous_) {
    box_.set_homogeneous(true);
    box_.set_hexpand(true);
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
    const std::vector<std::string> app_ids = mapping.getMemberNames();
    for (auto& app_id : app_ids) {
      app_ids_replace_map_.emplace(app_id, mapping[app_id].asString());
    }
  }

  ipc_.subscribe(R"(["window"])");
  ipc_.subscribe(R"(["workspace"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Taskbar::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Taskbar::onCmd));

  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    spdlog::error("Taskbar: {}", e.what());
  }

  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Taskbar: {}", e.what());
    }
  });
}

Taskbar::~Taskbar() {
  if (bar_css_states_) {
    setBarCssClass("toplevel-active", false);
    setBarCssClass("toplevel-fullscreen", false);
    setBarCssClass("toplevel-urgent", false);
  }
}

void Taskbar::onEvent(const struct Ipc::ipc_response& res) {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    spdlog::error("Taskbar: {}", e.what());
  }
}

void Taskbar::onCmd(const struct Ipc::ipc_response& res) {
  if (res.type != IPC_GET_TREE) {
    return;
  }
  try {
    // parser_ and the local snapshot are worker-thread only; only the handover
    // to windows_/current_workspace_ needs the lock, so the main thread never
    // blocks behind a full tree parse.
    auto payload = parser_.parse(res.payload);

    std::vector<TaskInfo> windows;
    std::string focused_workspace;
    walk_tree(payload, "", "", "", false, allOutputs(), bar_.output->name, app_ids_replace_map_,
              windows, focused_workspace);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      windows_ = std::move(windows);
      current_workspace_ = std::move(focused_workspace);
    }
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Taskbar: {}", e.what());
  }
}

void Taskbar::update() {
  std::vector<TaskInfo> windows;
  std::string current_workspace;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    windows = windows_;
    current_workspace = current_workspace_;
  }

  // Drop tasks that disappeared from the tree; ~Task() removes their button.
  tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                              [&windows](const TaskPtr& task) {
                                return std::none_of(windows.begin(), windows.end(),
                                                    [&task](const TaskInfo& info) {
                                                      return info.id == task->id();
                                                    });
                              }),
               tasks_.end());

  // Reconcile / create tasks for the snapshot, preserving tree order.
  std::vector<Task*> ordered;
  ordered.reserve(windows.size());
  for (const auto& info : windows) {
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                           [&info](const TaskPtr& task) { return task->id() == info.id; });
    Task* task = nullptr;
    if (it == tasks_.end()) {
      tasks_.push_back(std::make_unique<Task>(bar_, config_, this, info));
      task = tasks_.back().get();
    } else {
      (*it)->setData(info);
      task = it->get();
    }
    ordered.push_back(task);
  }

  // Ordering: tree order, optionally overridden by sort-by-app-id and/or
  // active-first, then overridden again by any persisted user drag order.
  if (sort_by_app_id_) {
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](Task* a, Task* b) { return a->app_id() < b->app_id(); });
  }

  if (active_first_) {
    auto it = std::find_if(ordered.begin(), ordered.end(), [](Task* t) { return t->active(); });
    if (it != ordered.end() && it != ordered.begin()) {
      std::rotate(ordered.begin(), it, std::next(it));
    }
  }

  if (!user_order_.empty()) {
    std::vector<Task*> final_order;
    final_order.reserve(ordered.size());
    for (int64_t id : user_order_) {
      auto it =
          std::find_if(ordered.begin(), ordered.end(), [id](Task* t) { return t->id() == id; });
      if (it != ordered.end()) {
        final_order.push_back(*it);
      }
    }
    for (Task* t : ordered) {
      if (std::find(final_order.begin(), final_order.end(), t) == final_order.end()) {
        final_order.push_back(t);
      }
    }
    ordered = std::move(final_order);
  }

  // Ignore-list / squash-list visibility. Computed here (not per-task) because
  // squashing must keep exactly one instance of a group visible: the first one
  // in display order stays, the rest are hidden. Runs after every task's data
  // has been refreshed via setData()/create above so duplicate counts are final.
  std::unordered_set<std::string> shown_groups;
  for (auto* task : ordered) {
    const bool ignored =
        ignore_list_.contains(task->app_id()) || ignore_list_.contains(task->title());
    bool squashed = false;
    if (!ignored) {
      // The squash group is identified by the squash-list entry that matched
      // (for "*", by the task's own app_id/title), and is qualified by which
      // field matched, so two unrelated apps cannot land in the same group just
      // because one's title equals the other's app_id.
      std::string group;
      bool duplicated = false;
      if (squash_list_.contains(task->app_id())) {
        group = "app_id:" + task->app_id();
        duplicated = taskAppIdCount(task->app_id()) > 1;
      } else if (squash_list_.contains(task->title())) {
        group = "title:" + task->title();
        duplicated = taskTitleCount(task->title()) > 1;
      } else if (squash_list_.contains("*")) {
        if (!task->app_id().empty()) {
          group = "app_id:" + task->app_id();
          duplicated = taskAppIdCount(task->app_id()) > 1;
        } else if (!task->title().empty()) {
          group = "title:" + task->title();
          duplicated = taskTitleCount(task->title()) > 1;
        }
      }
      if (!group.empty() && duplicated) {
        // Not the first shown member of the group -> squash.
        squashed = !shown_groups.insert(group).second;
      }
    }
    task->applyVisibility(ignored, squashed);
  }

  int pos = 0;
  for (auto* t : ordered) {
    if (t->visible()) {
      moveButton(t->button, pos++);
    }
  }

  // Render every task (labels/icon/tooltip/state classes, active-only visibility).
  for (auto* t : ordered) {
    t->update();
  }

  if (bar_css_states_) {
    bool has_active = false;
    bool has_fullscreen = false;
    bool has_urgent = false;
    for (auto* t : ordered) {
      if (!t->visible()) {
        continue;
      }
      if (t->active()) {
        has_active = true;
      }
      if (t->workspace() == current_workspace) {
        if (t->fullscreen()) {
          has_fullscreen = true;
        }
        if (t->urgent()) {
          has_urgent = true;
        }
      }
    }
    setBarCssClass("toplevel-active", has_active);
    setBarCssClass("toplevel-fullscreen", has_fullscreen);
    setBarCssClass("toplevel-urgent", has_urgent);
  }

  // Sole owner of the "empty" class: it tracks buttons that are actually
  // rendered, which with "active-only" is not the same as buttons packed into
  // the box. Runs after every add/remove path.
  const bool any_shown =
      std::any_of(ordered.begin(), ordered.end(), [](const Task* t) { return t->shown(); });
  if (any_shown) {
    box_.get_style_context()->remove_class("empty");
  } else {
    box_.get_style_context()->add_class("empty");
  }

  AModule::update();
}

void Taskbar::addButton(Gtk::Button& bt) {
  /* When "homogeneous" is enabled, let every child expand and fill so the buttons
   * divide the available width equally. Otherwise, only let the buttons expand to
   * fill the taskbar when "expand" is enabled and the bar is horizontal (see the
   * Task constructor for details). */
  const bool horizontal = bar_.orientation == Gtk::ORIENTATION_HORIZONTAL;
  if (homogeneous_) {
    box_.pack_start(bt, true, true);
    bt.set_hexpand(true);
    bt.set_halign(Gtk::ALIGN_FILL);
  } else if (expand_ && horizontal) {
    box_.pack_start(bt, true, true);
  } else {
    box_.pack_start(bt, false, false);
  }
}

void Taskbar::moveButton(Gtk::Button& bt, int pos) { box_.reorder_child(bt, pos); }

void Taskbar::removeButton(Gtk::Button& bt) { box_.remove(bt); }

void Taskbar::reorderTask(int64_t dragged_id, int64_t target_id) {
  if (dragged_id == target_id) {
    return;
  }

  const auto find = [this](int64_t id) -> Task* {
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                           [id](const TaskPtr& task) { return task->id() == id; });
    return it == tasks_.end() ? nullptr : it->get();
  };

  Task* dragged = find(dragged_id);
  Task* target = find(target_id);
  // The dragged window may have closed mid-drag, in which case its Task is
  // already gone: nothing to reorder.
  if (dragged == nullptr || target == nullptr || !dragged->visible() || !target->visible()) {
    return;
  }

  const auto position = box_.child_property_position(target->button).get_value();
  box_.reorder_child(dragged->button, position);

  recordUserOrder();
}

void Taskbar::recordUserOrder() {
  user_order_.clear();
  for (auto* child : box_.get_children()) {
    auto it = std::find_if(tasks_.begin(), tasks_.end(), [child](const TaskPtr& task) {
      return static_cast<Gtk::Widget*>(&task->button) == child;
    });
    if (it != tasks_.end()) {
      user_order_.push_back((*it)->id());
    }
  }
}

bool Taskbar::allOutputs() const {
  return config_["all-outputs"].isBool() && config_["all-outputs"].asBool();
}

Ipc& Taskbar::ipc() { return ipc_; }

const IconLoader& Taskbar::iconLoader() const { return icon_loader_; }

std::size_t Taskbar::taskAppIdCount(std::string_view app_id) const {
  return std::count_if(tasks_.begin(), tasks_.end(),
                       [app_id](const TaskPtr& task) { return app_id == task->app_id(); });
}

std::size_t Taskbar::taskTitleCount(std::string_view title) const {
  return std::count_if(tasks_.begin(), tasks_.end(),
                       [title](const TaskPtr& task) { return title == task->title(); });
}

void Taskbar::setBarCssClass(const std::string& class_name, bool enabled) {
  const auto style = bar_.window.get_style_context();
  if (enabled && !style->has_class(class_name)) {
    spdlog::trace("Adding bar class: {}", class_name);
    style->add_class(class_name);
  } else if (!enabled && style->has_class(class_name)) {
    spdlog::trace("Removing bar class: {}", class_name);
    style->remove_class(class_name);
  }
}

}  // namespace waybar::modules::sway
