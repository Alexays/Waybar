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

#include "util/gtk_icon.hpp"
#include "util/rewrite_string.hpp"

namespace waybar::modules::sway {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AAppIconLabel(config, "window", id, "{}", 0, true), bar_(bar), windowId_(-1) {
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
    updateAppIconName(app_id_, app_class_);
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Window: {}", e.what());
    spdlog::trace("Window::onCmd exception");
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
  AAppIconLabel::update();
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

std::optional<std::reference_wrapper<const Json::Value>> getSingleChildNode(
    const Json::Value& node) {
  auto const& nodes = node["nodes"];
  if (nodes.empty()) {
    if (node["type"].asString() == "workspace")
      return {};
    else if (node["type"].asString() == "floating_con") {
      return {};
    } else {
      return {std::cref(node)};
    }
  }
  auto it = std::cbegin(nodes);
  if (it == std::cend(nodes)) {
    return {};
  }
  auto const& child = *it;
  ++it;
  if (it != std::cend(nodes)) {
    return {};
  }
  return {getSingleChildNode(child)};
}

std::tuple<std::string, std::string, std::string> getWindowInfo(const Json::Value& node) {
  const auto app_id = node["app_id"].isString() ? node["app_id"].asString()
                                                : node["window_properties"]["instance"].asString();
  const auto app_class = node["window_properties"]["class"].isString()
                             ? node["window_properties"]["class"].asString()
                             : "";
  const auto shell = node["shell"].isString() ? node["shell"].asString() : "";
  return {app_id, app_class, shell};
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
      const auto [app_id, app_class, shell] = getWindowInfo(node);
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
    std::string app_id = "";
    std::string app_class = "";
    std::string workspace_layout = "";
    if (all_leaf_nodes.first == 1) {
      const auto single_child = getSingleChildNode(immediateParent);
      if (single_child.has_value()) {
        std::tie(app_id, app_class, workspace_layout) = getWindowInfo(single_child.value());
      }
    }
    return {all_leaf_nodes.first,
            all_leaf_nodes.second,
            0,
            (all_leaf_nodes.first > 0 || all_leaf_nodes.second > 0)
                ? config_["offscreen-css-text"].asString()
                : "",
            app_id,
            app_class,
            workspace_layout,
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
