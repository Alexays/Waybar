#include "modules/sway/window.hpp"

#include <gdkmm/pixbuf.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <gtkmm/enums.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <regex>
#include <string>

namespace waybar::modules::sway {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AIconLabel(config, "window", id, "{}", 0, true), bar_(bar), windowId_(-1) {
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
    }
  });
}

void Window::onEvent(const struct Ipc::ipc_response& res) { getTree(); }

void Window::onCmd(const struct Ipc::ipc_response& res) {
  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto payload = parser_.parse(res.payload);
    auto output = payload["output"].isString() ? payload["output"].asString() : "";
    std::tie(app_nb_, windowId_, window_, app_id_) = getFocusedNode(payload["nodes"], output);
    updateAppIcon();
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Window: {}", e.what());
  }
}

std::optional<std::string> getDesktopFilePath(const std::string& app_id) {
  const auto data_dirs = Glib::get_system_data_dirs();
  for (const auto& data_dir : data_dirs) {
    const auto desktop_file_path = data_dir + "applications/" + app_id + ".desktop";
    if (std::filesystem::exists(desktop_file_path)) {
      return desktop_file_path;
    }
  }
  return {};
}

std::optional<Glib::ustring> getIconName(const std::string& app_id) {
  const auto desktop_file_path = getDesktopFilePath(app_id);
  if (!desktop_file_path.has_value()) {
    return {};
  }
  try {
    Glib::KeyFile desktop_file;
    desktop_file.load_from_file(desktop_file_path.value());
    const auto icon_name = desktop_file.get_string("Desktop Entry", "Icon");
    if (icon_name.empty()) {
      return {};
    }
    return icon_name;
  } catch (Glib::FileError& error) {
    spdlog::warn(
        "Error while loading desktop file {}: {}", desktop_file_path.value(), error.what().c_str());
  } catch (Glib::KeyFileError& error) {
    spdlog::warn(
        "Error while loading desktop file {}: {}", desktop_file_path.value(), error.what().c_str());
  }
  return {};
}

void Window::updateAppIcon() {
  if (!iconEnabled()) {
    return;
  }
  const auto icon_name = getIconName(app_id_);
  if (icon_name.has_value()) {
    image_.set_from_icon_name(icon_name.value(), Gtk::ICON_SIZE_LARGE_TOOLBAR);
    image_.set_visible(true);
    return;
  }
  image_.set_visible(false);
}

auto Window::update() -> void {
  if (!old_app_id_.empty()) {
    bar_.window.get_style_context()->remove_class(old_app_id_);
  }
  if (app_nb_ == 0) {
    bar_.window.get_style_context()->remove_class("solo");
    if (!bar_.window.get_style_context()->has_class("empty")) {
      bar_.window.get_style_context()->add_class("empty");
    }
  } else if (app_nb_ == 1) {
    bar_.window.get_style_context()->remove_class("empty");
    if (!bar_.window.get_style_context()->has_class("solo")) {
      bar_.window.get_style_context()->add_class("solo");
    }
    if (!app_id_.empty() && !bar_.window.get_style_context()->has_class(app_id_)) {
      bar_.window.get_style_context()->add_class(app_id_);
      old_app_id_ = app_id_;
    }
  } else {
    bar_.window.get_style_context()->remove_class("solo");
    bar_.window.get_style_context()->remove_class("empty");
  }
  label_.set_markup(fmt::format(format_, fmt::arg("title", rewriteTitle(window_)),
                                fmt::arg("app_id", app_id_)));
  if (tooltipEnabled()) {
    label_.set_tooltip_text(window_);
  }
  // Call parent update
  AIconLabel::update();
}

int leafNodesInWorkspace(const Json::Value& node) {
  auto const& nodes = node["nodes"];
  auto const& floating_nodes = node["floating_nodes"];
  if(nodes.empty() && floating_nodes.empty()) {
    if(node["type"] == "workspace")
      return 0;
    else
      return 1;
  }
  int sum = 0;
  if (!nodes.empty()) {
    for(auto const& node : nodes)
      sum += leafNodesInWorkspace(node);
  }
  if (!floating_nodes.empty()) {
    for(auto const& node : floating_nodes)
      sum += leafNodesInWorkspace(node);
  }
  return sum;
}

std::tuple<std::size_t, int, std::string, std::string> gfnWithWorkspace(
    const Json::Value& nodes, std::string& output, const Json::Value& config_,
    const Bar& bar_, Json::Value& parentWorkspace) {
  for(auto const& node : nodes) {
    if (node["output"].isString()) {
      output = node["output"].asString();
    }
    // found node
    if (node["focused"].asBool() && (node["type"] == "con" || node["type"] == "floating_con")) {
      if ((!config_["all-outputs"].asBool() && output == bar_.output->name) ||
          config_["all-outputs"].asBool()) {
        auto app_id = node["app_id"].isString() ? node["app_id"].asString()
                      : node["window_properties"]["instance"].asString();
        int nb = node.size();
        if(parentWorkspace != 0)
          nb = leafNodesInWorkspace(parentWorkspace);
        return {nb,
          node["id"].asInt(),
          Glib::Markup::escape_text(node["name"].asString()),
          app_id};
      }
    }
    // iterate
    if(node["type"] == "workspace")
      parentWorkspace = node;
    auto [nb, id, name, app_id] = gfnWithWorkspace(node["nodes"], output, config_, bar_, parentWorkspace);
    if (id > -1 && !name.empty()) {
      return {nb, id, name, app_id};
    }
    // Search for floating node
    std::tie(nb, id, name, app_id) = gfnWithWorkspace(node["floating_nodes"], output, config_, bar_, parentWorkspace);
    if (id > -1 && !name.empty()) {
      return {nb, id, name, app_id};
    }
  }
  return {0, -1, "", ""};
}

std::tuple<std::size_t, int, std::string, std::string> Window::getFocusedNode(
    const Json::Value& nodes, std::string& output) {
  Json::Value placeholder = 0;
  return gfnWithWorkspace(nodes, output, config_, bar_, placeholder);
}

void Window::getTree() {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    spdlog::error("Window: {}", e.what());
  }
}

std::string Window::rewriteTitle(const std::string& title) {
  const auto& rules = config_["rewrite"];
  if (!rules.isObject()) {
    return title;
  }

  std::string res = title;

  for (auto it = rules.begin(); it != rules.end(); ++it) {
    if (it.key().isString() && it->isString()) {
      try {
        // malformated regexes will cause an exception.
        // in this case, log error and try the next rule.
        const std::regex rule{it.key().asString()};
        if (std::regex_match(title, rule)) {
          res = std::regex_replace(res, rule, it->asString());
        }
      } catch (const std::regex_error& e) {
        spdlog::error("Invalid rule {}: {}", it.key().asString(), e.what());
      }
    }
  }

  return res;
}

}  // namespace waybar::modules::sway
