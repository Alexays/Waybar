#include "modules/sway/window.hpp"

#include <gdkmm/pixbuf.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <gtkmm/enums.h>
#include <gtkmm/icontheme.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <regex>
#include <string>

#include "util/rewrite_string.hpp"

namespace waybar::modules::sway {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AIconLabel(config, "window", id, "{}", 0, true), bar_(bar), windowId_(-1) {
  // Icon size
  if (config_["icon-size"].isUInt()) {
    app_icon_size_ = config["icon-size"].asUInt();
  }
  image_.set_pixel_size(app_icon_size_);

  ipc_.subscribe(R"(["window","workspace"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Window::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Window::onCmd));
  // Get Initial focused window
  getTree();
  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Window: {}", e.what());
      spdlog::trace("Window::Window exception");
    }
  });
}

void Window::onEvent(const struct Ipc::ipc_response& res) { getTree(); }

void Window::onCmd(const struct Ipc::ipc_response& res) {
  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto payload = parser_.parse(res.payload);
    auto output = payload["output"].isString() ? payload["output"].asString() : "";
    std::tie(app_nb_, floating_count_, windowId_, window_, app_id_, app_class_, shell_, layout_) =
        getFocusedNode(payload["nodes"], output);
    updateAppIconName();
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Window: {}", e.what());
    spdlog::trace("Window::onCmd exception");
  }
}

std::optional<std::string> getDesktopFilePath(const std::string& app_id,
                                              const std::string& app_class) {
  const auto data_dirs = Glib::get_system_data_dirs();
  for (const auto& data_dir : data_dirs) {
    const auto data_app_dir = data_dir + "applications/";
    auto desktop_file_path = data_app_dir + app_id + ".desktop";
    if (std::filesystem::exists(desktop_file_path)) {
      return desktop_file_path;
    }
    if (!app_class.empty()) {
      desktop_file_path = data_app_dir + app_class + ".desktop";
      if (std::filesystem::exists(desktop_file_path)) {
        return desktop_file_path;
      }
    }
  }
  return {};
}

std::optional<Glib::ustring> getIconName(const std::string& app_id, const std::string& app_class) {
  const auto desktop_file_path = getDesktopFilePath(app_id, app_class);
  if (!desktop_file_path.has_value()) {
    // Try some heuristics to find a matching icon

    const auto default_icon_theme = Gtk::IconTheme::get_default();
    if (default_icon_theme->has_icon(app_id)) {
      return app_id;
    }

    const auto app_id_desktop = app_id + "-desktop";
    if (default_icon_theme->has_icon(app_id_desktop)) {
      return app_id_desktop;
    }

    const auto to_lower = [](const std::string& str) {
      auto str_cpy = str;
      std::transform(str_cpy.begin(), str_cpy.end(), str_cpy.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      return str;
    };

    const auto first_space = app_id.find_first_of(' ');
    if (first_space != std::string::npos) {
      const auto first_word = to_lower(app_id.substr(0, first_space));
      if (default_icon_theme->has_icon(first_word)) {
        return first_word;
      }
    }

    const auto first_dash = app_id.find_first_of('-');
    if (first_dash != std::string::npos) {
      const auto first_word = to_lower(app_id.substr(0, first_dash));
      if (default_icon_theme->has_icon(first_word)) {
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

  const auto icon_name = getIconName(app_id_, app_class_);
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
  spdlog::trace("workspace layout {}, tiled count {}, floating count {}", layout_, app_nb_,
                floating_count_);

  int mode = 0;
  if (app_nb_ == 0) {
    if (floating_count_ == 0) {
      mode += 1;
    } else {
      mode += 4;
    }
  } else if (app_nb_ == 1) {
    mode += 2;
  } else {
    if (layout_ == "tabbed") {
      mode += 8;
    } else if (layout_ == "stacked") {
      mode += 16;
    } else {
      mode += 32;
    }
  }

  if (!old_app_id_.empty() && ((mode & 2) == 0 || old_app_id_ != app_id_) &&
      bar_.window.get_style_context()->has_class(old_app_id_)) {
    spdlog::trace("Removing app_id class: {}", old_app_id_);
    bar_.window.get_style_context()->remove_class(old_app_id_);
    old_app_id_ = "";
  }

  setClass("empty", ((mode & 1) > 0));
  setClass("solo", ((mode & 2) > 0));
  setClass("floating", ((mode & 4) > 0));
  setClass("tabbed", ((mode & 8) > 0));
  setClass("stacked", ((mode & 16) > 0));
  setClass("tiled", ((mode & 32) > 0));

  if ((mode & 2) > 0 && !app_id_.empty() && !bar_.window.get_style_context()->has_class(app_id_)) {
    spdlog::trace("Adding app_id class: {}", app_id_);
    bar_.window.get_style_context()->add_class(app_id_);
    old_app_id_ = app_id_;
  }

  label_.set_markup(waybar::util::rewriteString(
      fmt::format(fmt::runtime(format_), fmt::arg("title", window_), fmt::arg("app_id", app_id_),
                  fmt::arg("shell", shell_)),
      config_["rewrite"]));
  if (tooltipEnabled()) {
    label_.set_tooltip_text(window_);
  }

  updateAppIcon();

  // Call parent update
  AIconLabel::update();
}

void Window::setClass(std::string classname, bool enable) {
  if (enable) {
    if (!bar_.window.get_style_context()->has_class(classname)) {
      bar_.window.get_style_context()->add_class(classname);
    }
  } else {
    bar_.window.get_style_context()->remove_class(classname);
  }
}

std::pair<int, int> leafNodesInWorkspace(const Json::Value& node) {
  auto const& nodes = node["nodes"];
  auto const& floating_nodes = node["floating_nodes"];
  if (nodes.empty() && floating_nodes.empty()) {
    if (node["type"].asString() == "workspace")
      return {0, 0};
    else if (node["type"].asString() == "floating_con") {
      return {0, 1};
    } else {
      return {1, 0};
    }
  }
  int sum = 0;
  int floating_sum = 0;
  for (auto const& node : nodes) {
    std::pair all_leaf_nodes = leafNodesInWorkspace(node);
    sum += all_leaf_nodes.first;
    floating_sum += all_leaf_nodes.second;
  }
  for (auto const& node : floating_nodes) {
    std::pair all_leaf_nodes = leafNodesInWorkspace(node);
    sum += all_leaf_nodes.first;
    floating_sum += all_leaf_nodes.second;
  }
  return {sum, floating_sum};
}

std::tuple<std::size_t, int, int, std::string, std::string, std::string, std::string, std::string>
gfnWithWorkspace(const Json::Value& nodes, std::string& output, const Json::Value& config_,
                 const Bar& bar_, Json::Value& parentWorkspace,
                 const Json::Value& immediateParent) {
  for (auto const& node : nodes) {
    if (node["type"].asString() == "output") {
      if ((!config_["all-outputs"].asBool() || config_["offscreen-css"].asBool()) &&
          (node["name"].asString() != bar_.output->name)) {
        continue;
      }
      output = node["name"].asString();
    } else if (node["type"].asString() == "workspace") {
      // needs to be a string comparison, because filterWorkspace is the current_workspace
      if (node["name"].asString() != immediateParent["current_workspace"].asString()) {
        continue;
      }
      if (node["focused"].asBool()) {
        std::pair all_leaf_nodes = leafNodesInWorkspace(node);
        return {all_leaf_nodes.first,
                all_leaf_nodes.second,
                node["id"].asInt(),
                (((all_leaf_nodes.first > 0) || (all_leaf_nodes.second > 0)) &&
                 (config_["show-focused-workspace-name"].asBool()))
                    ? node["name"].asString()
                    : "",
                "",
                "",
                "",
                node["layout"].asString()};
      }
      parentWorkspace = node;
    } else if ((node["type"].asString() == "con" || node["type"].asString() == "floating_con") &&
               (node["focused"].asBool())) {
      // found node
      spdlog::trace("actual output {}, output found {}, node (focused) found {}", bar_.output->name,
                    output, node["name"].asString());
      auto app_id = node["app_id"].isString() ? node["app_id"].asString()
                                              : node["window_properties"]["instance"].asString();
      const auto app_class = node["window_properties"]["class"].isString()
                                 ? node["window_properties"]["class"].asString()
                                 : "";
      const auto shell = node["shell"].isString() ? node["shell"].asString() : "";
      int nb = node.size();
      int floating_count = 0;
      std::string workspace_layout = "";
      if (!parentWorkspace.isNull()) {
        std::pair all_leaf_nodes = leafNodesInWorkspace(parentWorkspace);
        nb = all_leaf_nodes.first;
        floating_count = all_leaf_nodes.second;
        workspace_layout = parentWorkspace["layout"].asString();
      }
      return {nb,
              floating_count,
              node["id"].asInt(),
              Glib::Markup::escape_text(node["name"].asString()),
              app_id,
              app_class,
              shell,
              workspace_layout};
    }

    // iterate
    auto [nb, f, id, name, app_id, app_class, shell, workspace_layout] =
        gfnWithWorkspace(node["nodes"], output, config_, bar_, parentWorkspace, node);
    auto [nb2, f2, id2, name2, app_id2, app_class2, shell2, workspace_layout2] =
        gfnWithWorkspace(node["floating_nodes"], output, config_, bar_, parentWorkspace, node);

    //    if ((id > 0 || ((id2 < 0 || name2.empty()) && id > -1)) && !name.empty()) {
    if ((id > 0) || (id2 < 0 && id > -1)) {
      return {nb, f, id, name, app_id, app_class, shell, workspace_layout};
    } else if (id2 > 0 && !name2.empty()) {
      return {nb2, f2, id2, name2, app_id2, app_class, shell2, workspace_layout2};
    }
  }

  // this only comes into effect when no focused children are present
  if (config_["all-outputs"].asBool() && config_["offscreen-css"].asBool() &&
      immediateParent["type"].asString() == "workspace") {
    std::pair all_leaf_nodes = leafNodesInWorkspace(immediateParent);
    // using an empty string as default ensures that no window depending styles are set due to the
    // checks above for !name.empty()
    return {all_leaf_nodes.first,
            all_leaf_nodes.second,
            0,
            (all_leaf_nodes.first > 0 || all_leaf_nodes.second > 0)
                ? config_["offscreen-css-text"].asString()
                : "",
            "",
            "",
            "",
            immediateParent["layout"].asString()};
  }

  return {0, 0, -1, "", "", "", "", ""};
}

std::tuple<std::size_t, int, int, std::string, std::string, std::string, std::string, std::string>
Window::getFocusedNode(const Json::Value& nodes, std::string& output) {
  Json::Value placeholder = Json::Value::null;
  return gfnWithWorkspace(nodes, output, config_, bar_, placeholder, placeholder);
}

void Window::getTree() {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    spdlog::error("Window: {}", e.what());
    spdlog::trace("Window::getTree exception");
  }
}

}  // namespace waybar::modules::sway
