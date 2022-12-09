#include "modules/wnd.hpp"

#include <spdlog/spdlog.h>

#include <iostream>

#include "modules/wnd/utils/process.hpp"

namespace waybar::modules {
Wnd::Wnd(const std::string& id, const Json::Value& config)
    : ALabel(config, "wnd", id, "{app} cpu {cpu}% mem: {vmRss}Mb", 1, true),
      windowId_(-1),
      display_(ALabel::format_) {
  ipc_.subscribe(R"(["window","workspace"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Wnd::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Wnd::onCmd));
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

  this->thread_ = [this] {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ALabel::dp.emit();
    }
    auto now = std::chrono::system_clock::now();
    auto diff = now.time_since_epoch() % ALabel::interval_;
    this->thread_.sleep_for(ALabel::interval_ - diff);
  };
}

auto Wnd::update() -> void {
  // Call parent update
  wnd::utils::ProcessTree::Process processTree =
      wnd::utils::ProcessTree::get_tree_for_process(std::to_string(windowId_));
  ALabel::label_.set_markup(this->display_.show_head(processTree));
  ALabel::label_.set_tooltip_markup(this->display_.show(processTree));
  ALabel::update();
}

void Wnd::getTree() {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    spdlog::error("Window: {}", e.what());
  }
}

void Wnd::onEvent(const struct sway::Ipc::ipc_response& res) { getTree(); }

void Wnd::onCmd(const struct sway::Ipc::ipc_response& res) {
  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto payload = parser_.parse(res.payload);
    std::tie(app_nb_, windowId_, window_) = getFocusedNode(payload["nodes"]);
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Window: {}", e.what());
  }
}

int leafNodesInWorkspace(const Json::Value& node) {
  auto const& nodes = node["nodes"];
  auto const& floating_nodes = node["floating_nodes"];
  if (nodes.empty() && floating_nodes.empty()) {
    if (node["type"] == "workspace")
      return 0;
    else
      return 1;
  }
  int sum = 0;
  if (!nodes.empty()) {
    for (auto const& node : nodes) sum += leafNodesInWorkspace(node);
  }
  if (!floating_nodes.empty()) {
    for (auto const& node : floating_nodes) sum += leafNodesInWorkspace(node);
  }
  return sum;
}

std::tuple<std::size_t, int, std::string> gfnWithWorkspace(const Json::Value& nodes,
                                                           const Json::Value& config_,
                                                           Json::Value& parentWorkspace) {
  for (auto const& node : nodes) {
    // found node
    if (node["focused"].asBool() && (node["type"] == "con" || node["type"] == "floating_con")) {
      auto app_id = node["pid"].isInt() ? node["pid"].asInt() : -1;
      int nb = node.size();
      if (parentWorkspace != 0) nb = leafNodesInWorkspace(parentWorkspace);
      return {nb, app_id, Glib::Markup::escape_text(node["name"].asString())};
    }

    if (node["type"] == "workspace") parentWorkspace = node;
    auto [nb, id, name] = gfnWithWorkspace(node["nodes"], config_, parentWorkspace);
    if (id > -1 && !name.empty()) {
      return {nb, id, name};
    }
    // Search for floating node
    std::tie(nb, id, name) = gfnWithWorkspace(node["floating_nodes"], config_, parentWorkspace);
    if (id > -1 && !name.empty()) {
      return {nb, id, name};
    }
  }

  return {0, -1, ""};
}

std::tuple<std::size_t, int, std::string> Wnd::getFocusedNode(const Json::Value& nodes) {
  Json::Value placeholder = 0;
  return gfnWithWorkspace(nodes, config_, placeholder);
}
}  // namespace waybar::modules
