#include "modules/niri/workspace.hpp"

#include <giomm/desktopappinfo.h>
#include <giomm/themedicon.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/icontheme.h>
#include <spdlog/spdlog.h>

#include <algorithm>

#include "modules/niri/backend.hpp"
#include "modules/niri/workspaces.hpp"
#include "util/gtk_icon.hpp"
#include "util/tooltip.hpp"

namespace waybar::modules::niri {

Workspace::Workspace(const Json::Value& workspace_data, Workspaces& manager)
    : manager_(manager),
      id_(workspace_data["id"].asUInt64()),
      box_(Gtk::Orientation::HORIZONTAL, 0),
      taskbar_box_(Gtk::Orientation::HORIZONTAL, 0) {
  button_.set_child(box_);
  box_.append(label_);
  box_.append(taskbar_box_);

  button_.set_has_frame(false);
  button_.add_css_class("niri-workspace");

  if (!manager_.config()["disable-click"].asBool()) {
    const auto ws_id = id_;
    auto click = Gtk::GestureClick::create();
    click->signal_pressed().connect([ws_id](int, double, double) {
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
    });
    button_.add_controller(click);
  }
}

void Workspace::update(const Json::Value& data, const std::vector<Json::Value>& all_windows,
                       const std::string& windows_str, std::size_t total) {
  // ── CSS classes ──────────────────────────────────────────────────────────
  auto setClass = [&](const char* cls, bool on) {
    if (on)
      button_.add_css_class(cls);
    else
      button_.remove_css_class(cls);
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
    button_.set_visible(data[prop].asBool());
  } else if (cfg["hide-empty"].asBool()) {
    button_.set_visible(!data["active_window_id"].isNull() || data["is_focused"].asBool());
  } else {
    button_.set_visible(true);
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
    taskbar_box_.set_visible(true);
    label_.set_visible(false);
  } else {
    taskbar_buttons_.clear();
    taskbar_box_.set_visible(false);
  }
}

// ── Taskbar rebuild ──────────────────────────────────────────────────────────

void Workspace::rebuildTaskbar(const std::vector<Json::Value>& my_windows) {
  std::erase_if(taskbar_buttons_, [&](const auto& entry) {
    return std::none_of(my_windows.begin(), my_windows.end(), [&](const Json::Value& win) {
      return win["id"].asUInt64() == entry.first;
    });
  });

  const auto& taskbar_cfg = manager_.config()["workspace-taskbar"];
  const int icon_size = taskbar_cfg["icon-size"].isInt() ? taskbar_cfg["icon-size"].asInt() : 16;

  Gtk::Widget* previous = nullptr;
  for (const auto& win : my_windows) {
    const auto win_id = win["id"].asUInt64();
    const std::string app_id = win["app_id"].isString() ? win["app_id"].asString() : "";
    const std::string title = win["title"].isString() ? win["title"].asString() : app_id;
    const bool is_focused = win["is_focused"].asBool();

    const auto [it, is_new] = taskbar_buttons_.try_emplace(win_id);
    auto& entry = it->second;
    auto& btn = entry.button;
    if (is_new) {
      btn.set_has_frame(false);
      btn.add_css_class("niri-taskbar-btn");
      taskbar_box_.append(btn);
    }
    if (previous != nullptr) {
      taskbar_box_.reorder_child_after(btn, *previous);
    } else {
      taskbar_box_.reorder_child_at_start(btn);
    }
    previous = &btn;

    if (is_focused) {
      btn.add_css_class("focused");
    } else {
      btn.remove_css_class("focused");
    }
    util::set_tooltip_markup(btn, Glib::Markup::escape_text(title));

    if (is_new || entry.app_id != app_id) {
      entry.app_id = app_id;
      auto icon = loadIcon(app_id);
      if (icon) {
        entry.image.set(icon);
        entry.image.set_pixel_size(icon_size);
      }
      Gtk::Widget& child = icon ? static_cast<Gtk::Widget&>(entry.image) : entry.label;
      if (btn.get_child() != &child) {
        btn.set_child(child);
      }
    }
    if (btn.get_child() == &entry.label) {
      // Three characters, not bytes, and none that break or widen the line
      std::string fallback;
      const char* p = (app_id.empty() ? title : app_id).c_str();
      for (int n = 0; n < 3 && *p != '\0'; p = g_utf8_next_char(p)) {
        const gunichar c = g_utf8_get_char_validated(p, -1);
        if (c == static_cast<gunichar>(-1) || c == static_cast<gunichar>(-2)) break;
        if (g_unichar_iscntrl(c) || c == 0x2028 || c == 0x2029) continue;
        fallback.append(p, g_utf8_next_char(p) - p);
        ++n;
      }
      if (fallback.empty()) fallback = "?";
      if (entry.label.get_text() != fallback) entry.label.set_text(fallback);
    }

    if (!is_new) continue;

    // Left click → focus window.
    btn.signal_clicked().connect([win_id] {
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

    // Middle click closes the window. The gesture only reports a release whose press it saw.
    auto middle = Gtk::GestureClick::create();
    middle->set_button(GDK_BUTTON_MIDDLE);
    middle->signal_released().connect([win_id, gesture = middle.get()](int, double, double) {
      gesture->set_state(Gtk::EventSequenceState::CLAIMED);
      try {
        Json::Value request(Json::objectValue);
        auto& action = (request["Action"] = Json::Value(Json::objectValue));
        auto& closeWindow = (action["CloseWindow"] = Json::Value(Json::objectValue));
        closeWindow["id"] = win_id;
        IPC::send(request);
      } catch (const std::exception& e) {
        spdlog::error("Niri: error closing window {}: {}", win_id, e.what());
      }
    });
    btn.add_controller(middle);
  }
}

// ── Icon loading ─────────────────────────────────────────────────────────────

Glib::RefPtr<Gio::Icon> Workspace::loadIcon(const std::string& app_id) {
  if (app_id.empty()) return {};
  auto app_info = Gio::DesktopAppInfo::create(app_id + ".desktop");
  if (app_info) {
    auto icon = app_info->get_icon();
    if (icon && DefaultGtkIconThemeWrapper::theme()->has_gicon(icon)) return icon;
  }

  auto tryLoad = [](const std::string& name) -> Glib::RefPtr<Gio::Icon> {
    if (!DefaultGtkIconThemeWrapper::has_icon(name)) return {};
    return Gio::ThemedIcon::create(name);
  };

  if (auto icon = tryLoad(app_id)) return icon;

  std::string lower = app_id;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if (auto icon = tryLoad(lower)) return icon;

  auto dot = app_id.rfind('.');
  if (dot != std::string::npos) {
    std::string last = app_id.substr(dot + 1);
    std::transform(last.begin(), last.end(), last.begin(), ::tolower);
    if (auto icon = tryLoad(last)) return icon;
  }

  return {};
}

}  // namespace waybar::modules::niri