#include "modules/niri/workspace.hpp"

#include <gdkmm/pixbuf.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <spdlog/spdlog.h>
#include <giomm/desktopappinfo.h>
#include <giomm/icon.h>

#include "modules/niri/backend.hpp"
#include "modules/niri/workspaces.hpp"

namespace waybar::modules::niri {

Workspace::Workspace(const Json::Value& workspace_data, Workspaces& manager)
    : manager_(manager),
      id_(workspace_data["id"].asUInt64()),
      box_(Gtk::ORIENTATION_HORIZONTAL, 0),
      taskbar_box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  button_.add(box_);
  box_.pack_start(label_, false, false, 0);
  box_.pack_start(taskbar_box_, false, false, 0);

  button_.set_relief(Gtk::RELIEF_NONE);
  button_.get_style_context()->add_class("niri-workspace");

  if (!manager_.config()["disable-click"].asBool()) {
    const auto ws_id = id_;
    button_.signal_pressed().connect([ws_id] {
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
  }

  button_.show_all();
}

void Workspace::update(const Json::Value& data, const std::vector<Json::Value>& all_windows) {
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
                       fmt::arg("output", data["output"].asString()));
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
    btn->set_relief(Gtk::RELIEF_NONE);
    btn->get_style_context()->add_class("niri-taskbar-btn");
    if (is_focused) btn->get_style_context()->add_class("focused");
    btn->set_tooltip_text(title);

    auto pixbuf = loadIcon(app_id, icon_size);
    if (pixbuf) {
      auto* img = Gtk::make_managed<Gtk::Image>(pixbuf);
      btn->add(*img);
    } else {
      std::string fallback = app_id.empty() ? title : app_id;
      if (!fallback.empty()) {
        fallback = fallback.substr(0, 3);
      } else {
        fallback = "?";
      }
      auto* lbl = Gtk::make_managed<Gtk::Label>(fallback);
      btn->add(*lbl);
    }

    // Left click → focus window.
    btn->signal_clicked().connect([win_id] {
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
    btn->signal_button_release_event().connect([win_id](GdkEventButton* event) -> bool {
      if (event->button == GDK_BUTTON_MIDDLE) {
        try {
          Json::Value request(Json::objectValue);
          auto& action = (request["Action"] = Json::Value(Json::objectValue));
          auto& closeWindow = (action["CloseWindow"] = Json::Value(Json::objectValue));
          closeWindow["id"] = win_id;
          IPC::send(request);
        } catch (const std::exception& e) {
          spdlog::error("Niri: error closing window {}: {}", win_id, e.what());
        }
        return true;
      }
      return false;
    });

    taskbar_box_.pack_start(*btn, false, false, 0);
    btn->show_all();
  }
}

// ── Icon loading ─────────────────────────────────────────────────────────────

Glib::RefPtr<Gdk::Pixbuf> Workspace::loadIcon(const std::string& app_id, int size) {
    if (app_id.empty()) return {};
    auto app_info = Gio::DesktopAppInfo::create(app_id + ".desktop");
        
    if (app_info) {
        auto icon = app_info->get_icon();
        if (icon) {
          auto theme = Gtk::IconTheme::get_default();
          auto icon_info = theme->lookup_icon(icon, size, Gtk::ICON_LOOKUP_FORCE_SIZE);
        
          if (icon_info) {
              try {
                  
                  return icon_info.load_icon(); 
              } catch (...) {
                
              }
          }
      }
    }

    auto theme = Gtk::IconTheme::get_default();
    
    auto tryLoad = [&](const std::string& name) -> Glib::RefPtr<Gdk::Pixbuf> {
        if (!theme->has_icon(name)) return {};
        try {
            return theme->load_icon(name, size, Gtk::ICON_LOOKUP_FORCE_SIZE);
        } catch (...) {
            return {};
        }
    };

    if (auto pb = tryLoad(app_id)) return pb;

    std::string lower = app_id;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (auto pb = tryLoad(lower)) return pb;

    auto dot = app_id.rfind('.');
    if (dot != std::string::npos) {
        std::string last = app_id.substr(dot + 1);
        std::transform(last.begin(), last.end(), last.begin(), ::tolower);
        if (auto pb = tryLoad(last)) return pb;
    }

    return {};
}

}  // namespace waybar::modules::niri