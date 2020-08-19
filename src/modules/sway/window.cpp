#include "modules/sway/window.hpp"
#include <spdlog/spdlog.h>

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
  label_.set_text(fmt::format(format_, window_));
  if (tooltipEnabled()) {
    label_.set_tooltip_text(window_);
  }
  // Call parent update
  ALabel::update();
}

std::tuple<std::size_t, int, std::string, std::string> Window::getFocusedNode(
    const Json::Value& nodes, std::string& output) {
  for (auto const& node : nodes) {
    if (node["output"].isString()) {
      output = node["output"].asString();
    }
    if (node["focused"].asBool() && (node["type"] == "con" || node["type"] == "floating_con")) {
      if ((!config_["all-outputs"].asBool() && output == bar_.output->name) ||
          config_["all-outputs"].asBool()) {
        auto app_id = node["app_id"].isString() ? node["app_id"].asString()
                                                : node["window_properties"]["instance"].asString();
        return {nodes.size(),
                node["id"].asInt(),
                Glib::Markup::escape_text(node["name"].asString()),
                app_id};
      }
    }
    auto [nb, id, name, app_id] = getFocusedNode(node["nodes"], output);
    if (id > -1 && !name.empty()) {
      return {nb, id, name, app_id};
    }
    // Search for floating node
    std::tie(nb, id, name, app_id) = getFocusedNode(node["floating_nodes"], output);
    if (id > -1 && !name.empty()) {
      return {nb, id, name, app_id};
    }
  }
  return {0, -1, "", ""};
}

void Window::getTree() {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    spdlog::error("Window: {}", e.what());
  }
}

}  // namespace waybar::modules::sway
