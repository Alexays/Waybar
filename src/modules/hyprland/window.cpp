#include "modules/hyprland/window.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <regex>
#include <util/sanitize_str.hpp>
#include <vector>

#include "modules/hyprland/backend.hpp"
#include "util/gtk_icon.hpp"
#include "util/json.hpp"
#include "util/rewrite_string.hpp"

namespace waybar::modules::hyprland {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AIconLabel(config, "window", id, "{title}", 0, true), bar_(bar) {
  modulesReady = true;
  separate_outputs = config["separate-outputs"].asBool();

  // Icon size
  if (config["icon-size"].isUInt()) {
    app_icon_size_ = config["icon-size"].asUInt();
  }
  image_.set_pixel_size(app_icon_size_);

  if (!gIPC.get()) {
    gIPC = std::make_unique<IPC>();
  }

  queryActiveWorkspace();
  update();

  // register for hyprland ipc
  gIPC->registerForIPC("activewindow", this);
  gIPC->registerForIPC("closewindow", this);
  gIPC->registerForIPC("movewindow", this);
  gIPC->registerForIPC("changefloatingmode", this);
  gIPC->registerForIPC("fullscreen", this);
}

Window::~Window() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

std::optional<std::string> getDesktopFilePath(const std::string& app_class) {
  const auto data_dirs = Glib::get_system_data_dirs();
  for (const auto& data_dir : data_dirs) {
    const auto data_app_dir = data_dir + "applications/";
    auto desktop_file_path = data_app_dir + app_class + ".desktop";
    if (std::filesystem::exists(desktop_file_path)) {
      return desktop_file_path;
    }
  }
  return {};
}

std::optional<Glib::ustring> getIconName(const std::string& app_class) {
  const auto desktop_file_path = getDesktopFilePath(app_class);
  if (!desktop_file_path.has_value()) {
    // Try some heuristics to find a matching icon

    if (DefaultGtkIconThemeWrapper::has_icon(app_class)) {
      return app_class;
    }

    const auto app_identifier_desktop = app_class + "-desktop";
    if (DefaultGtkIconThemeWrapper::has_icon(app_identifier_desktop)) {
      return app_identifier_desktop;
    }

    const auto to_lower = [](const std::string& str) {
      auto str_cpy = str;
      std::transform(str_cpy.begin(), str_cpy.end(), str_cpy.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      return str;
    };

    const auto first_space = app_class.find_first_of(' ');
    if (first_space != std::string::npos) {
      const auto first_word = to_lower(app_class.substr(0, first_space));
      if (DefaultGtkIconThemeWrapper::has_icon(first_word)) {
        return first_word;
      }
    }

    const auto first_dash = app_class.find_first_of('-');
    if (first_dash != std::string::npos) {
      const auto first_word = to_lower(app_class.substr(0, first_dash));
      if (DefaultGtkIconThemeWrapper::has_icon(first_word)) {
        return first_word;
      }
    }

    return {};
  }

  try {
    Glib::KeyFile desktop_file;
    desktop_file.load_from_file(desktop_file_path.value());
    return desktop_file.get_string("Desktop Entry", "Icon");
  } catch (Glib::FileError& error) {
    spdlog::warn("Error while loading desktop file {}: {}", desktop_file_path.value(),
                 error.what().c_str());
  } catch (Glib::KeyFileError& error) {
    spdlog::warn("Error while loading desktop file {}: {}", desktop_file_path.value(),
                 error.what().c_str());
  }
  return {};
}

void Window::updateAppIconName() {
  if (!iconEnabled()) {
    return;
  }

  const auto icon_name = getIconName(window_data_.class_name);
  if (icon_name.has_value()) {
    app_icon_name_ = icon_name.value();
  } else {
    app_icon_name_ = "";
  }
  update_app_icon_ = true;
}

void Window::updateAppIcon() {
  if (update_app_icon_) {
    update_app_icon_ = false;
    if (app_icon_name_.empty()) {
      image_.set_visible(false);
    } else {
      image_.set_from_icon_name(app_icon_name_, Gtk::ICON_SIZE_INVALID);
      image_.set_visible(true);
    }
  }
}

auto Window::update() -> void {
  // fix ampersands
  std::lock_guard<std::mutex> lg(mutex_);

  std::string window_name = waybar::util::sanitize_string(workspace_.last_window_title);
  std::string window_address = workspace_.last_window;

  if (window_name != window_data_.title) {
    if (window_name.empty()) {
      label_.get_style_context()->add_class("empty");
    } else {
      label_.get_style_context()->remove_class("empty");
    }
    window_data_.title = window_name;
  }

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(waybar::util::rewriteString(
        fmt::format(fmt::runtime(format_), fmt::arg("title", window_name),
                    fmt::arg("initialTitle", window_data_.initial_title),
                    fmt::arg("class", window_data_.class_name),
                    fmt::arg("initialClass", window_data_.initial_class_name)),
        config_["rewrite"]));
  } else {
    label_.hide();
  }

  setClass("empty", workspace_.windows == 0);
  setClass("solo", solo_);
  setClass("floating", all_floating_);
  setClass("hidden", hidden_);
  setClass("fullscreen", fullscreen_);

  if (!last_solo_class_.empty() && solo_class_ != last_solo_class_) {
    if (bar_.window.get_style_context()->has_class(last_solo_class_)) {
      bar_.window.get_style_context()->remove_class(last_solo_class_);
      spdlog::trace("Removing solo class: {}", last_solo_class_);
    }
  }

  if (!solo_class_.empty() && solo_class_ != last_solo_class_) {
    bar_.window.get_style_context()->add_class(solo_class_);
    spdlog::trace("Adding solo class: {}", solo_class_);
  }
  last_solo_class_ = solo_class_;

  updateAppIcon();

  ALabel::update();
}

auto Window::getActiveWorkspace() -> Workspace {
  const auto workspace = gIPC->getSocket1JsonReply("activeworkspace");
  assert(workspace.isObject());
  return Workspace::parse(workspace);
}

auto Window::getActiveWorkspace(const std::string& monitorName) -> Workspace {
  const auto monitors = gIPC->getSocket1JsonReply("monitors");
  assert(monitors.isArray());
  auto monitor = std::find_if(monitors.begin(), monitors.end(),
                              [&](Json::Value monitor) { return monitor["name"] == monitorName; });
  if (monitor == std::end(monitors)) {
    spdlog::warn("Monitor not found: {}", monitorName);
    return Workspace{-1, 0, "", ""};
  }
  const int id = (*monitor)["activeWorkspace"]["id"].asInt();

  const auto workspaces = gIPC->getSocket1JsonReply("workspaces");
  assert(workspaces.isArray());
  auto workspace = std::find_if(workspaces.begin(), workspaces.end(),
                                [&](Json::Value workspace) { return workspace["id"] == id; });
  if (workspace == std::end(workspaces)) {
    spdlog::warn("No workspace with id {}", id);
    return Workspace{-1, 0, "", ""};
  }
  return Workspace::parse(*workspace);
}

auto Window::Workspace::parse(const Json::Value& value) -> Window::Workspace {
  return Workspace{value["id"].asInt(), value["windows"].asInt(), value["lastwindow"].asString(),
                   value["lastwindowtitle"].asString()};
}

auto Window::WindowData::parse(const Json::Value& value) -> Window::WindowData {
  return WindowData{value["floating"].asBool(), value["monitor"].asInt(),
                    value["class"].asString(),  value["initialClass"].asString(),
                    value["title"].asString(),  value["initialTitle"].asString()};
}

void Window::queryActiveWorkspace() {
  std::lock_guard<std::mutex> lg(mutex_);

  if (separate_outputs) {
    workspace_ = getActiveWorkspace(this->bar_.output->name);
  } else {
    workspace_ = getActiveWorkspace();
  }

  if (workspace_.windows > 0) {
    const auto clients = gIPC->getSocket1JsonReply("clients");
    assert(clients.isArray());
    auto active_window = std::find_if(clients.begin(), clients.end(), [&](Json::Value window) {
      return window["address"] == workspace_.last_window;
    });
    if (active_window == std::end(clients)) {
      return;
    }

    window_data_ = WindowData::parse(*active_window);
    updateAppIconName();
    std::vector<Json::Value> workspace_windows;
    std::copy_if(clients.begin(), clients.end(), std::back_inserter(workspace_windows),
                 [&](Json::Value window) {
                   return window["workspace"]["id"] == workspace_.id && window["mapped"].asBool();
                 });
    hidden_ = std::any_of(workspace_windows.begin(), workspace_windows.end(),
                          [&](Json::Value window) { return window["hidden"].asBool(); });
    std::vector<Json::Value> visible_windows;
    std::copy_if(workspace_windows.begin(), workspace_windows.end(),
                 std::back_inserter(visible_windows),
                 [&](Json::Value window) { return !window["hidden"].asBool(); });
    solo_ = 1 == std::count_if(visible_windows.begin(), visible_windows.end(),
                               [&](Json::Value window) { return !window["floating"].asBool(); });
    all_floating_ = std::all_of(visible_windows.begin(), visible_windows.end(),
                                [&](Json::Value window) { return window["floating"].asBool(); });
    fullscreen_ = (*active_window)["fullscreen"].asBool();

    if (fullscreen_) {
      solo_ = true;
    }

    if (solo_) {
      solo_class_ = window_data_.class_name;
    } else {
      solo_class_ = "";
    }
  } else {
    window_data_ = WindowData{};
    all_floating_ = false;
    hidden_ = false;
    fullscreen_ = false;
    solo_ = false;
    solo_class_ = "";
  }
}

void Window::onEvent(const std::string& ev) {
  queryActiveWorkspace();

  dp.emit();
}

void Window::setClass(const std::string& classname, bool enable) {
  if (enable) {
    if (!bar_.window.get_style_context()->has_class(classname)) {
      bar_.window.get_style_context()->add_class(classname);
    }
  } else {
    bar_.window.get_style_context()->remove_class(classname);
  }
}

}  // namespace waybar::modules::hyprland
