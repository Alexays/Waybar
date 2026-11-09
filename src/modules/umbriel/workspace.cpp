#include "modules/umbriel/workspace.hpp"

#include <fmt/format.h>
#include <gdkmm/pixbuf.h>
#include <giomm/desktopappinfo.h>
#include <giomm/icon.h>
#include <gtkmm/icontheme.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <string>

#include "modules/umbriel/backend.hpp"
#include "modules/umbriel/workspaces.hpp"
#include "util/tooltip.hpp"

namespace waybar::modules::umbriel {

Workspace::Workspace(const Json::Value& workspace_data, Workspaces& manager)
    : manager_(manager),
      id_(workspace_data["id"].asString()),
      name_(workspace_data["name"].asString()),
      output_(workspace_data["output"].asString()),
      index_(workspace_data["index"].asUInt()),
      named_(workspace_data["named"].asBool()),
      box_(Gtk::ORIENTATION_HORIZONTAL, 0),
      taskbar_box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  button_.add(box_);
  box_.pack_start(label_, false, false, 0);
  box_.pack_start(taskbar_box_, false, false, 0);

  // Keep the button content centered when the button is wider than its
  // natural size (for example when a min-width is set in CSS).
  box_.set_halign(Gtk::ALIGN_CENTER);
  box_.set_valign(Gtk::ALIGN_CENTER);
  label_.set_xalign(0.5F);
  label_.set_justify(Gtk::JUSTIFY_CENTER);

  button_.set_relief(Gtk::RELIEF_NONE);
  button_.get_style_context()->add_class("umbriel-workspace");

  if (!manager_.config()["disable-click"].asBool()) {
    button_.signal_pressed().connect([this] {
      try {
        Json::Value request(Json::objectValue);
        request["cmd"] = "action";
        request["action"] = "workspace-switch";

        // Umbriel addresses a workspace either by its display name (even a
        // numeric one) or by its 1-based position on an output.
        if (named_ && !name_.empty()) {
          request["workspace"] = name_;
        } else {
          request["workspace"] = index_;
        }
        if (!output_.empty()) {
          request["output"] = output_;
        }

        IPC::send(request);
      } catch (const std::exception& e) {
        spdlog::error("Umbriel: error switching workspace: {}", e.what());
      }
    });
  }

  button_.show_all();
}

void Workspace::update(const Json::Value& data, const std::vector<Json::Value>& all_windows,
                       const std::string& windows_str, std::size_t total) {
  name_ = data["name"].asString();
  output_ = data["output"].asString();
  index_ = data["index"].asUInt();
  named_ = data["named"].asBool();

  auto style = button_.get_style_context();

  const auto setClass = [&](const char* cls, bool on) {
    if (on) {
      style->add_class(cls);
    } else {
      style->remove_class(cls);
    }
  };

  setClass("focused", data["focused"].asBool());
  setClass("active", data["active"].asBool());
  setClass("urgent", data["urgent"].asBool());
  setClass("empty", data["window_count"].asInt() == 0);
  setClass("current_output",
           data["output"].isString() && data["output"].asString() == manager_.bar().output->name);

  std::string name =
      data["name"].isString() ? data["name"].asString() : std::to_string(data["index"].asUInt());

  button_.set_name("umbriel-workspace-" + name);

  const auto& cfg = manager_.config();

  if (cfg["format"].isString()) {
    const auto format = cfg["format"].asString();
    name = fmt::format(fmt::runtime(format), fmt::arg("icon", manager_.getIcon(name, data)),
                       fmt::arg("value", name), fmt::arg("name", data["name"].asString()),
                       fmt::arg("index", data["index"].asUInt()),
                       fmt::arg("output", data["output"].asString()), fmt::arg("total", total),
                       fmt::arg("layout", data["layout"].asString()),
                       fmt::arg("windows", windows_str));
  }

  if (!cfg["disable-markup"].asBool()) {
    label_.set_markup(name);
  } else {
    label_.set_text(name);
  }

  const bool alloutputs = cfg["all-outputs"].asBool();
  if (cfg["current-only"].asBool()) {
    const auto* prop = alloutputs ? "focused" : "active";
    data[prop].asBool() ? button_.show() : button_.hide();
  } else if (cfg["hide-empty"].asBool()) {
    (data["window_count"].asInt() == 0 && !data["focused"].asBool()) ? button_.hide()
                                                                     : button_.show();
  } else {
    button_.show();
  }

  const auto& taskbar_cfg = cfg["workspace-taskbar"];
  if (taskbar_cfg.isObject() && taskbar_cfg["enable"].asBool()) {
    std::vector<Json::Value> my_windows;
    for (const auto& win : all_windows) {
      if (win["workspace"].asString() == id_) {
        my_windows.push_back(win);
      }
    }

    std::sort(my_windows.begin(), my_windows.end(), [](const Json::Value& a, const Json::Value& b) {
      const auto a_column = a["column"].asInt();
      const auto b_column = b["column"].asInt();
      const auto a_row = a["row"].asInt();
      const auto b_row = b["row"].asInt();

      if (a_column >= 0 && b_column >= 0) {
        if (a_column != b_column) return a_column < b_column;
        if (a_row != b_row) return a_row < b_row;
      } else {
        const auto a_x = a["x"].asInt();
        const auto b_x = b["x"].asInt();
        if (a_x != b_x) return a_x < b_x;
        const auto a_y = a["y"].asInt();
        const auto b_y = b["y"].asInt();
        if (a_y != b_y) return a_y < b_y;
      }

      return a["id"].asString() < b["id"].asString();
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

void Workspace::rebuildTaskbar(const std::vector<Json::Value>& my_windows) {
  for (auto* child : taskbar_box_.get_children()) {
    taskbar_box_.remove(*child);
  }

  const auto& taskbar_cfg = manager_.config()["workspace-taskbar"];
  const int icon_size = taskbar_cfg["icon-size"].isInt() ? taskbar_cfg["icon-size"].asInt() : 16;

  for (const auto& win : my_windows) {
    const auto win_id = win["id"].asString();
    const std::string app_id = win["app_id"].isString() ? win["app_id"].asString() : "";
    const std::string title = win["title"].isString() && !win["title"].asString().empty()
                                  ? win["title"].asString()
                                  : app_id;
    const bool is_focused = win["focused"].asBool();

    auto* btn = Gtk::make_managed<Gtk::Button>();
    btn->set_relief(Gtk::RELIEF_NONE);
    btn->get_style_context()->add_class("umbriel-taskbar-btn");
    if (is_focused) btn->get_style_context()->add_class("focused");
    std::string tooltip = title;
    util::limit_tooltip_text(tooltip);
    btn->set_tooltip_text(tooltip);

    auto pixbuf = loadIcon(app_id, icon_size);
    if (pixbuf) {
      auto* img = Gtk::make_managed<Gtk::Image>(pixbuf);
      btn->add(*img);
    } else {
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
      auto* lbl = Gtk::make_managed<Gtk::Label>(fallback);
      btn->add(*lbl);
    }

    if (!win_id.empty()) {
      // Left click → focus window.
      btn->signal_clicked().connect([win_id] {
        try {
          Json::Value request(Json::objectValue);
          request["cmd"] = "action";
          request["action"] = "window-focus";
          request["window"] = win_id;
          IPC::send(request);
        } catch (const std::exception& e) {
          spdlog::error("Umbriel: error focusing window {}: {}", win_id, e.what());
        }
      });

      // Middle click → close window.
      btn->signal_button_release_event().connect([win_id](GdkEventButton* event) -> bool {
        if (event->button == GDK_BUTTON_MIDDLE) {
          try {
            Json::Value request(Json::objectValue);
            request["cmd"] = "action";
            request["action"] = "window-close";
            request["window"] = win_id;
            IPC::send(request);
          } catch (const std::exception& e) {
            spdlog::error("Umbriel: error closing window {}: {}", win_id, e.what());
          }
          return true;
        }
        return false;
      });
    }

    taskbar_box_.pack_start(*btn, false, false, 0);
    btn->show_all();
  }
}

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
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (auto pb = tryLoad(lower)) return pb;

  const auto dot = app_id.rfind('.');
  if (dot != std::string::npos) {
    std::string last = app_id.substr(dot + 1);
    std::transform(last.begin(), last.end(), last.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (auto pb = tryLoad(last)) return pb;
  }

  return {};
}

}  // namespace waybar::modules::umbriel
