#include "modules/sway/window.hpp"
#include <spdlog/spdlog.h>
#include <regex>

namespace waybar::modules::sway {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "window", id, "{}", 0, true), bar_(bar), windowId_(-1) {
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
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Window: {}", e.what());
  }
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
  ALabel::update();
}

int leafNodesInWorkspace(const Json::Value& node) {
  auto const& nodes = node["nodes"];
  if(nodes.empty()) {
    if(node["type"] == "workspace")
      return 0;
    else
      return 1;
  }
  int sum = 0;
  for(auto const& node : nodes)
    sum += leafNodesInWorkspace(node);
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

std::string Window::rewriteTitle(const std::string& title)
{
  const auto& rules = config_["rewrite"];
  if (!rules.isObject()) {
    return title;
  }

  for (auto it = rules.begin(); it != rules.end(); ++it) {
    if (it.key().isString() && it->isString()) {
      const std::regex rule{it.key().asString()};
      if (std::regex_match(title, rule)) {
        return std::regex_replace(title, rule, it->asString());
      }
    }
  }

  return title;
}

}  // namespace waybar::modules::sway
