#include "modules/niri/workspaces.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>

#include "util/command.hpp"
#include "util/string.hpp"

namespace waybar::modules::niri {

std::string getWorkspaceName(const Json::Value& workspace_data) {
  if (workspace_data["name"]) return workspace_data["name"].asString();
  return std::to_string(workspace_data["idx"].asUInt());
}

Workspace::Workspace(const Json::Value& config, const uint64_t id, const std::string& name)
    : config_(config), id_(id), name_(name) {
  button_.set_name("niri-workspace-" + name_);

  taskBarConfig_ = config_["workspace-taskbar"];
  if (taskBarConfig_.get("enable", false).asBool())
    content_.pack_start(label_, false, false);
  else
    content_.set_center_widget(label_);
  // label_.set_label(name_);

  label_.get_style_context()->add_class("workspace-label");
  button_.set_relief(Gtk::RELIEF_NONE);
  if (!config_["disable-click"].asBool()) {
    button_.signal_pressed().connect([=] {
      try {
        // {"Action":{"FocusWorkspace":{"reference":{"Id":1}}}}
        Json::Value request(Json::objectValue);
        auto& action = (request["Action"] = Json::Value(Json::objectValue));
        auto& focusWorkspace = (action["FocusWorkspace"] = Json::Value(Json::objectValue));
        auto& reference = (focusWorkspace["reference"] = Json::Value(Json::objectValue));
        reference["Id"] = id_;

        IPC::send(request);
      } catch (const std::exception& e) {
        spdlog::error("Error switching workspace: {}", e.what());
      }
    });
  }
  button_.add(content_);
}

std::string Workspace::getIcon(const std::string& value, const Json::Value& ws) {
  const auto& icons = config_["format-icons"];
  if (!icons) return value;

  if (ws["is_urgent"].asBool() && icons["urgent"]) return icons["urgent"].asString();

  if (ws["active_window_id"].isNull() && icons["empty"]) return icons["empty"].asString();

  if (ws["is_focused"].asBool() && icons["focused"]) return icons["focused"].asString();

  if (ws["is_active"].asBool() && icons["active"]) return icons["active"].asString();

  if (ws["name"]) {
    const auto& name = ws["name"].asString();
    if (icons[name]) return icons[name].asString();
  }

  const auto idx = ws["idx"].asString();
  if (icons[idx]) return icons[idx].asString();

  if (icons["default"]) return icons["default"].asString();

  return value;
}

void Workspace::updateTaskbar(const std::vector<Json::Value>& windows_data,
                              const uint64_t active_window_id) {
  if (!taskBarConfig_.get("enable", false).asBool()) return;

  for (auto child : content_.get_children()) {
    if (child != &label_) {
      content_.remove(*child);
      // despite the remove, still needs a delete to prevent memory leak. Speculating that this
      // might work differently in GTK4.
      delete child;
    }
  }

  auto separator = taskBarConfig_.get("separator", " ").asString();

  auto format = taskBarConfig_.get("format", "{icon}").asString();
  bool taskbarWithIcon = false;
  std::string taskbarFormatBefore, taskbarFormatAfter;

  if (format != "") {
    auto parts = split(format, "{icon}", 1);
    taskbarFormatBefore = parts[0];
    if (parts.size() > 1) {
      taskbarWithIcon = true;
      taskbarFormatAfter = parts[1];
    }
  } else {
    taskbarWithIcon = true;  // default to icon-only
  }

  auto format_tooltip = taskBarConfig_.get("tooltip-format", "{title}").asString();

  auto sorted_windows_data = windows_data;
  std::sort(sorted_windows_data.begin(), sorted_windows_data.end(),
            [](const Json::Value& a, const Json::Value& b) {
              auto layoutA = a["layout"];
              auto layoutB = b["layout"];

              // Handle null positions (floating windows)
              if (layoutA["pos_in_scrolling_layout"].isNull()) {
                return false;  // Floating windows go to the end
              }
              if (layoutB["pos_in_scrolling_layout"].isNull()) {
                return true;  // Tiled windows before floating
              }

              // Both are tiled windows - sort by position
              return layoutA["pos_in_scrolling_layout"][0].asInt() <
                     layoutB["pos_in_scrolling_layout"][0].asInt();
            });
  int window_count = 0;
  for (const auto& window : sorted_windows_data) {
    if (window["workspace_id"].asUInt64() != id_) continue;
    if (window_count++ != 0 && !separator.empty()) {
      auto windowSeparator = Gtk::make_managed<Gtk::Label>(separator);
      content_.pack_start(*windowSeparator, false, false);
    }

    auto window_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL);
    if (!format_tooltip.empty()) {
      auto txt =
          fmt::format(fmt::runtime(format_tooltip), fmt::arg("title", window["title"].asString()),
                      fmt::arg("app_id", window["app_id"].asString()));
      window_box->set_tooltip_text(txt);
    }
    window_box->get_style_context()->add_class("taskbar-window");
    if (window["is_focused"].asBool()) window_box->get_style_context()->add_class("focused");
    if (window["is_floating"].asBool()) window_box->get_style_context()->add_class("floating");
    if (window["is_urgent"].asBool()) window_box->get_style_context()->add_class("urgent");
    if (window["id"].asUInt64() == active_window_id)
      window_box->get_style_context()->add_class("active");
    auto event_box = Gtk::make_managed<Gtk::EventBox>();
    event_box->add(*window_box);
    if (!config_["disable-click"].asBool()) {
      auto window_click_lambda_func = [](GdkEventButton* event, const uint64_t window_id) -> bool {
        if (event->type == GDK_BUTTON_PRESS) {
          try {
            // {"Action":{"FocusWindow":{"reference":{"Id":1}}}}
            Json::Value request(Json::objectValue);
            auto& action = (request["Action"] = Json::Value(Json::objectValue));
            auto& focusWindow = (action["FocusWindow"] = Json::Value(Json::objectValue));
            focusWindow["id"] = window_id;

            IPC::send(request);
          } catch (const std::exception& e) {
            spdlog::error("Error focusing window: {}", e.what());
            return false;
          }
        }
        return true;
      };
      bool (*func_ptr)(GdkEventButton*, const uint64_t) = window_click_lambda_func;
      event_box->signal_button_press_event().connect(
          sigc::bind(sigc::ptr_fun(func_ptr), window["id"].asUInt64()));
    }

    auto text_before = fmt::format(fmt::runtime(taskbarFormatBefore),
                                   fmt::arg("title", window["title"].asString()),
                                   fmt::arg("app_id", window["app_id"].asString()));
    if (!text_before.empty()) {
      auto window_label_before = Gtk::make_managed<Gtk::Label>(text_before);
      window_box->pack_start(*window_label_before, true, true);
    }

    if (taskbarWithIcon) {
      auto window_icon = Gtk::make_managed<Gtk::Image>();
      iconLoader_.image_load_icon(
          *window_icon, IconLoader::get_app_info_from_app_id_list(window["app_id"].asString()),
          taskBarConfig_.get("icon-size", 16).asInt());
      window_box->pack_start(*window_icon, false, false);
    }

    auto text_after =
        fmt::format(fmt::runtime(taskbarFormatAfter), fmt::arg("title", window["title"].asString()),
                    fmt::arg("app_id", window["app_id"].asString()));
    if (!text_after.empty()) {
      auto window_label_after = Gtk::make_managed<Gtk::Label>(text_after);
      window_box->pack_start(*window_label_after, true, true);
    }

    content_.pack_start(*event_box, false, false);
  }
}

void Workspace::update(const Json::Value& workspace_data,
                       const std::vector<Json::Value>& windows_data, const std::string& display) {
  auto style_context = button_.get_style_context();

  if (workspace_data["is_focused"].asBool())
    style_context->add_class("focused");
  else
    style_context->remove_class("focused");

  if (workspace_data["is_active"].asBool())
    style_context->add_class("active");
  else
    style_context->remove_class("active");

  if (workspace_data["is_urgent"].asBool())
    style_context->add_class("urgent");
  else
    style_context->remove_class("urgent");

  if (workspace_data["output"]) {
    if (workspace_data["output"].asString() == display)
      style_context->add_class("current_output");
    else
      style_context->remove_class("current_output");
  } else {
    style_context->remove_class("current_output");
  }

  if (workspace_data["active_window_id"].isNull())
    style_context->add_class("empty");
  else
    style_context->remove_class("empty");

  std::string name = getWorkspaceName(workspace_data);
  if (config_["format"].isString()) {
    auto format = config_["format"].asString();
    name = fmt::format(fmt::runtime(format), fmt::arg("icon", getIcon(name, workspace_data)),
                       fmt::arg("value", name), fmt::arg("name", workspace_data["name"].asString()),
                       fmt::arg("index", workspace_data["idx"].asUInt()),
                       fmt::arg("output", workspace_data["output"].asString()));
  }
  if (!config_["disable-markup"].asBool())
    label_.set_markup(name);
  else
    label_.set_text(name);

  updateTaskbar(windows_data, workspace_data["active_window_id"].asUInt64());

  if (config_["current-only"].asBool()) {
    const auto* property = config_["all-outputs"].asBool() ? "is_focused" : "is_active";
    if (workspace_data[property].asBool())
      button_.show_all();
    else
      button_.hide();
  } else
    button_.show_all();
}

Workspaces::Workspaces(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "workspaces", id, false, false), bar_(bar), box_(bar.orientation, 0) {
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  if (!gIPC) gIPC = std::make_unique<IPC>();

  gIPC->registerForIPC("WorkspacesChanged", this);
  gIPC->registerForIPC("WorkspaceActivated", this);
  gIPC->registerForIPC("WorkspaceActiveWindowChanged", this);
  gIPC->registerForIPC("WorkspaceUrgencyChanged", this);
  gIPC->registerForIPC("WindowFocusChanged", this);
  gIPC->registerForIPC("WindowOpenedOrChanged", this);
  gIPC->registerForIPC("WindowClosed", this);
  gIPC->registerForIPC("WindowLayoutsChanged", this);

  dp.emit();
}

Workspaces::~Workspaces() { gIPC->unregisterForIPC(this); }

void Workspaces::onEvent(const Json::Value& ev) { dp.emit(); }

void Workspaces::doUpdate() {
  auto ipcLock = gIPC->lockData();

  const auto alloutputs = config_["all-outputs"].asBool();
  std::vector<Json::Value> my_workspaces;
  const auto& workspaces = gIPC->workspaces();
  std::copy_if(workspaces.cbegin(), workspaces.cend(), std::back_inserter(my_workspaces),
               [&](const auto& ws) {
                 if (alloutputs) return true;
                 return ws["output"].asString() == bar_.output->name;
               });

  // Remove buttons for removed workspaces.
  for (auto it = workspaces_.begin(); it != workspaces_.end();) {
    auto ws = std::find_if(my_workspaces.begin(), my_workspaces.end(),
                           [it](const auto& ws) { return ws["id"].asUInt64() == it->first; });
    if (ws == my_workspaces.end()) {
      it = workspaces_.erase(it);
    } else {
      ++it;
    }
  }

  const auto& windows = gIPC->windows();
  // Add buttons for new workspaces, update existing ones.
  for (const auto& ws : my_workspaces) {
    std::vector<Json::Value> my_windows;
    std::copy_if(windows.cbegin(), windows.cend(), std::back_inserter(my_windows),
                 [&](const auto& win) {
                   if (alloutputs) return true;
                   return win["workspace_id"].asUInt64() == ws["id"].asUInt64();
                 });
    const auto ws_id = ws["id"].asUInt64();
    auto found_ws = workspaces_.find(ws_id);
    if (found_ws == workspaces_.end()) addWorkspace(ws, my_windows);
    workspaces_.at(ws_id)->update(ws, my_windows, bar_.output->name);
  }

  for (auto it = my_workspaces.cbegin(); it != my_workspaces.cend(); ++it) {
    const auto& ws = *it;

    auto pos = ws["idx"].asUInt() - 1;
    if (alloutputs) pos = it - my_workspaces.cbegin();

    auto& button = workspaces_[ws["id"].asUInt64()]->button();
    box_.reorder_child(button, pos);
  }
}

void Workspaces::update() {
  doUpdate();
  AModule::update();
}

void Workspaces::addWorkspace(const Json::Value& workspace_data,
                              const std::vector<Json::Value>& windows_data) {
  const auto new_workspace_id = workspace_data["id"].asUInt64();

  auto new_workspace =
      std::make_unique<Workspace>(config_, new_workspace_id, getWorkspaceName(workspace_data));
  box_.pack_start(new_workspace->button(), false, false, 0);
  workspaces_[new_workspace_id] = std::move(new_workspace);
}

}  // namespace waybar::modules::niri
