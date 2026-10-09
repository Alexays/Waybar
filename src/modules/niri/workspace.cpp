#include "modules/niri/workspace.hpp"

#include <gdkmm/pixbuf.h>
#include <giomm/desktopappinfo.h>
#include <giomm/icon.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <spdlog/spdlog.h>

#include "modules/niri/backend.hpp"
#include "modules/niri/workspaces.hpp"
#include "util/gtk/gtk_icon.hpp"

namespace waybar::modules::niri {

Workspace::Workspace(const Json::Value& workspace_data, Workspaces& manager)
    : manager_(manager),
      id_(workspace_data["id"].asUInt64()),
      box_(Gtk::Orientation::HORIZONTAL, 0),
      taskbar_box_(Gtk::Orientation::HORIZONTAL, 0) {
  button_.set_child(box_);
  box_.prepend(label_);
  box_.prepend(taskbar_box_);

  button_.add_css_class("flat");
  button_.get_style_context()->add_class("niri-workspace");

  if (!manager_.config()["disable-click"].asBool()) {
    const auto ws_id = id_;

    auto gesture{Gtk::GestureClick::create()};
    gesture->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    gesture->set_button(0u);
    button_.add_controller(gesture);
    gesture->signal_pressed().connect(
        [ws_id](int n_press, double x, double y) {
          try {
            Json::Value request(Json::objectValue);
            auto& action = (request["Action"] = Json::Value(Json::objectValue));
            auto& focusWorkspace = (action["FocusWorkspace"] = Json::Value(Json::objectValue));
            auto& reference = (focusWorkspace["reference"] = Json::Value(Json::objectValue));
            reference["Id"] = ws_id;
            IPC::send(request);
          } catch (const std::exception& e) {
            spdlog::error("Niri: error focusing workspace: {}", e.what());
          }
        },
        true);
  }

  button_.show();
}

void Workspace::update(const Json::Value& data, const std::vector<Json::Value>& all_windows,
                       const std::string& windows_str, std::size_t total) {
  // ── CSS classes ──────────────────────────────────────────────────────────
  auto style = button_.get_style_context();

  auto setClass = [&](const char* cls, bool on) {
    if (on)
      style->add_class(cls);
    else
      style->remove_class(cls);
  };

  setClass("focused", data["is_focused"].asBool());
  setClass("active", data["is_active"].asBool());
  setClass("urgent", data["is_urgent"].asBool());
  setClass("empty", data["active_window_id"].isNull());
  setClass("current_output",
           data["output"] && data["output"].asString() == manager_.bar().output->name);

  // ── Workspace label ───────────────────────────────────────────────────────
  std::string name;
  if (data["name"]) {
    name = data["name"].asString();
  } else {
    name = std::to_string(data["idx"].asUInt());
  }

  button_.set_name("niri-workspace-" + name);

  const auto& cfg = manager_.config();

  if (cfg["format"].isString()) {
    auto format = cfg["format"].asString();
    name = fmt::format(fmt::runtime(format), fmt::arg("icon", manager_.getIcon(name, data)),
                       fmt::arg("value", name), fmt::arg("name", data["name"].asString()),
                       fmt::arg("index", data["idx"].asUInt()),
                       fmt::arg("output", data["output"].asString()), fmt::arg("total", total),
                       fmt::arg("windows", windows_str));
  }

  if (!cfg["disable-markup"].asBool()) {
    label_.set_markup(name);
  } else {
    label_.set_text(name);
  }

  // ── Visibility ───────────────────────────────────────────────────────────
  const bool alloutputs = cfg["all-outputs"].asBool();
  if (cfg["current-only"].asBool()) {
    const auto* prop = alloutputs ? "is_focused" : "is_active";
    data[prop].asBool() ? button_.show() : button_.hide();
  } else if (cfg["hide-empty"].asBool()) {
    (data["active_window_id"].isNull() && !data["is_focused"].asBool()) ? button_.hide()
                                                                        : button_.show();
  } else {
    button_.show();
  }

  // ── Taskbar ───────────────────────────────────────────────────────────────
  const auto& taskbar_cfg = cfg["workspace-taskbar"];
  if (taskbar_cfg.isObject() && taskbar_cfg["enable"].asBool()) {
    std::vector<Json::Value> my_windows;
    for (const auto& win : all_windows) {
      if (win["workspace_id"].asUInt64() == id_) {
        my_windows.push_back(win);
      }
    }

    std::sort(my_windows.begin(), my_windows.end(), [](const Json::Value& a, const Json::Value& b) {
      const auto& la = a["layout"];
      const auto& lb = b["layout"];
      const bool ha = la.isObject() && la["pos_in_scrolling_layout"].isArray();
      const bool hb = lb.isObject() && lb["pos_in_scrolling_layout"].isArray();
      if (!ha && !hb) return false;
      if (!ha) return false;
      if (!hb) return true;
      const int col_a = la["pos_in_scrolling_layout"][0].asInt();
      const int col_b = lb["pos_in_scrolling_layout"][0].asInt();
      if (col_a != col_b) return col_a < col_b;
      return la["pos_in_scrolling_layout"][1].asInt() < lb["pos_in_scrolling_layout"][1].asInt();
    });

    rebuildTaskbar(my_windows);
    taskbar_box_.show();
    label_.hide();
  } else {
    for (auto* child : taskbar_box_.get_children()) {
      taskbar_box_.remove(*child);
    }
    taskbar_box_.hide();
  }
}

// ── Taskbar rebuild ──────────────────────────────────────────────────────────

void Workspace::rebuildTaskbar(const std::vector<Json::Value>& my_windows) {
  for (auto* child : taskbar_box_.get_children()) {
    taskbar_box_.remove(*child);
  }

  const auto& taskbar_cfg = manager_.config()["workspace-taskbar"];
  const int icon_size = taskbar_cfg["icon-size"].isInt() ? taskbar_cfg["icon-size"].asInt() : 16;

  for (const auto& win : my_windows) {
    const auto win_id = win["id"].asUInt64();
    const std::string app_id = win["app_id"].isString() ? win["app_id"].asString() : "";
    const std::string title = win["title"].isString() ? win["title"].asString() : app_id;
    const bool is_focused = win["is_focused"].asBool();

    auto* btn = Gtk::make_managed<Gtk::Button>();
    btn->add_css_class("flat");
    btn->get_style_context()->add_class("niri-taskbar-btn");
    if (is_focused) btn->get_style_context()->add_class("focused");
    btn->set_tooltip_text(title);

    Gtk::Image icon;
    const auto app_info{util::gtk::HIcon::get_app_info_by_list(app_id)};
    if (util::gtk::HIcon::image_load_icon(icon, app_info, icon_size)) {
      btn->set_child(icon);
    } else {
      std::string fallback = app_id.empty() ? title : app_id;
      if (!fallback.empty()) {
        fallback = fallback.substr(0, 3);
      } else {
        fallback = "?";
      }
      auto* lbl = Gtk::make_managed<Gtk::Label>(fallback);
      btn->set_child(*lbl);
    }

    auto gesture{Gtk::GestureClick::create()};
    gesture->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    gesture->set_button(0u);
    btn->add_controller(gesture);

    // Left click → focus window.
    gesture->signal_pressed().connect([win_id](int n_press, double x, double y) {
      try {
        Json::Value request(Json::objectValue);
        auto& action = (request["Action"] = Json::Value(Json::objectValue));
        auto& focusWindow = (action["FocusWindow"] = Json::Value(Json::objectValue));
        focusWindow["id"] = win_id;
        IPC::send(request);
      } catch (const std::exception& e) {
        spdlog::error("Niri: error focusing window {}: {}", win_id, e.what());
      }
    });

    // Middle click → close window.
    gesture->signal_released().connect([win_id, gesture](int n_press, double x, double y) {
      if (gesture->get_current_event()->get_button() == GDK_BUTTON_MIDDLE) {
        try {
          Json::Value request(Json::objectValue);
          auto& action = (request["Action"] = Json::Value(Json::objectValue));
          auto& closeWindow = (action["CloseWindow"] = Json::Value(Json::objectValue));
          closeWindow["id"] = win_id;
          IPC::send(request);
        } catch (const std::exception& e) {
          spdlog::error("Niri: error closing window {}: {}", win_id, e.what());
        }
      }
    });

    taskbar_box_.prepend(*btn);
    btn->show();
  }
}

}  // namespace waybar::modules::niri
