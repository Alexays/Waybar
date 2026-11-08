#include "modules/umbriel/workspace.hpp"

#include <fmt/format.h>
#include <giomm/desktopappinfo.h>
#include <giomm/themedicon.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/icontheme.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <string>

#include "modules/umbriel/backend.hpp"
#include "modules/umbriel/workspaces.hpp"
#include "util/gtk_icon.hpp"
#include "util/tooltip.hpp"

namespace waybar::modules::umbriel {

Workspace::Workspace(const Json::Value& workspace_data, Workspaces& manager)
    : manager_(manager),
      id_(workspace_data["id"].asString()),
      name_(workspace_data["name"].asString()),
      output_(workspace_data["output"].asString()),
      index_(workspace_data["index"].asUInt()),
      named_(workspace_data["named"].asBool()),
      box_(Gtk::Orientation::HORIZONTAL, 0),
      taskbar_box_(Gtk::Orientation::HORIZONTAL, 0) {
  button_.set_child(box_);
  box_.append(label_);
  box_.append(taskbar_box_);

  // Keep the button content centered when the button is wider than its
  // natural size (for example when a min-width is set in CSS).
  box_.set_halign(Gtk::Align::CENTER);
  box_.set_valign(Gtk::Align::CENTER);
  label_.set_xalign(0.5F);
  label_.set_justify(Gtk::Justification::CENTER);

  button_.set_has_frame(false);
  button_.add_css_class("umbriel-workspace");

  if (!manager_.config()["disable-click"].asBool()) {
    auto click = Gtk::GestureClick::create();
    click->signal_pressed().connect([this](int, double, double) {
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
    button_.add_controller(click);
  }
}

void Workspace::update(const Json::Value& data, const std::vector<Json::Value>& all_windows,
                       const std::string& windows_str, std::size_t total) {
  name_ = data["name"].asString();
  output_ = data["output"].asString();
  index_ = data["index"].asUInt();
  named_ = data["named"].asBool();

  const auto setClass = [&](const char* cls, bool on) {
    if (on) {
      button_.add_css_class(cls);
    } else {
      button_.remove_css_class(cls);
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
    button_.set_visible(data[prop].asBool());
  } else if (cfg["hide-empty"].asBool()) {
    button_.set_visible(data["window_count"].asInt() != 0 || data["focused"].asBool());
  } else {
    button_.set_visible(true);
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
    taskbar_box_.set_visible(true);
    label_.set_visible(false);
  } else {
    while (auto* child = taskbar_box_.get_first_child()) {
      taskbar_box_.remove(*child);
    }
    taskbar_box_.set_visible(false);
  }
}

void Workspace::rebuildTaskbar(const std::vector<Json::Value>& my_windows) {
  while (auto* child = taskbar_box_.get_first_child()) {
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
    btn->set_has_frame(false);
    btn->add_css_class("umbriel-taskbar-btn");
    if (is_focused) btn->add_css_class("focused");
    std::string tooltip = title;
    util::limit_tooltip_text(tooltip);
    btn->set_tooltip_text(tooltip);

    if (auto icon = loadIcon(app_id)) {
      auto* img = Gtk::make_managed<Gtk::Image>(icon);
      img->set_pixel_size(icon_size);
      btn->set_child(*img);
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
      btn->set_child(*Gtk::make_managed<Gtk::Label>(fallback));
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
      auto middle = Gtk::GestureClick::create();
      middle->set_button(GDK_BUTTON_MIDDLE);
      middle->signal_released().connect([win_id, gesture = middle.get()](int, double, double) {
        gesture->set_state(Gtk::EventSequenceState::CLAIMED);
        try {
          Json::Value request(Json::objectValue);
          request["cmd"] = "action";
          request["action"] = "window-close";
          request["window"] = win_id;
          IPC::send(request);
        } catch (const std::exception& e) {
          spdlog::error("Umbriel: error closing window {}: {}", win_id, e.what());
        }
      });
      btn->add_controller(middle);
    }

    taskbar_box_.append(*btn);
  }
}

Glib::RefPtr<Gio::Icon> Workspace::loadIcon(const std::string& app_id) {
  if (app_id.empty()) return {};

  auto app_info = Gio::DesktopAppInfo::create(app_id + ".desktop");
  if (app_info) {
    auto icon = app_info->get_icon();
    if (icon && DefaultGtkIconThemeWrapper::theme()->has_gicon(icon)) return icon;
  }

  const auto tryLoad = [](const std::string& name) -> Glib::RefPtr<Gio::Icon> {
    if (!DefaultGtkIconThemeWrapper::has_icon(name)) return {};
    return Gio::ThemedIcon::create(name);
  };

  if (auto icon = tryLoad(app_id)) return icon;

  std::string lower = app_id;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (auto icon = tryLoad(lower)) return icon;

  const auto dot = app_id.rfind('.');
  if (dot != std::string::npos) {
    std::string last = app_id.substr(dot + 1);
    std::transform(last.begin(), last.end(), last.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (auto icon = tryLoad(last)) return icon;
  }

  return {};
}

}  // namespace waybar::modules::umbriel
