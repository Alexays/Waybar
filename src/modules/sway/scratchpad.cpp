#include "modules/sway/scratchpad.hpp"

#include <spdlog/spdlog.h>

#include <string>

namespace waybar::modules::sway {
Scratchpad::Scratchpad(const std::string& id, const waybar::Bar& bar, const Json::Value& config)
    : ALabel(config, "scratchpad", id, "{count}"),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
      bar_(bar),
      count_(0) {
  ipc_.subscribe(R"(["window"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Scratchpad::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Scratchpad::onCmd));

  getTree();

  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Scratchpad: {}", e.what());
    }
  });
}
auto Scratchpad::update() -> void {
  label_.set_markup(fmt::format(format_, fmt::arg("count", count_)));
  AModule::update();
}

auto Scratchpad::getTree() -> void {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    spdlog::error("Scratchpad: {}", e.what());
  }
}

auto Scratchpad::onCmd(const struct Ipc::ipc_response& res) -> void {
  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto tree = parser_.parse(res.payload);
    count_ = tree["nodes"][0]["nodes"][0]["floating_nodes"].size();
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Scratchpad: {}", e.what());
  }
}

auto Scratchpad::onEvent(const struct Ipc::ipc_response& res) -> void { getTree(); }
}  // namespace waybar::modules::sway