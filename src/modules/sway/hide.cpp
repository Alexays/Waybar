#include "modules/sway/hide.hpp"
#include <spdlog/spdlog.h>
#include "client.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

namespace waybar::modules::sway {

Hide::Hide(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "hide", id, "{}", 0, true), bar_(bar), windowId_(-1) {
  ipc_.subscribe(R"(["bar_state_update","barconfig_update"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Hide::onEvent));
  // Do not reserve space for the bar anymore
  bar.removeExclusiveZone();
  // Launch worker
  worker();
}

void Hide::onEvent(const struct Ipc::ipc_response& res) {
  auto payload = parser_.parse(res.payload);
  std::lock_guard<std::mutex> lock(mutex_);
  if (payload.isMember("mode")) {
    // barconfig_update: get mode
    auto mode = payload["mode"].asString();
    if (mode == "hide") {
      // Hide the bars when configuring the "hide" bar
      for (auto& bar : waybar::Client::inst()->bars) {
        bar->setVisible(false);
      }
    }
  } else if (payload.isMember("visible_by_modifier")) {
    // bar_state_update: get visible_by_modifier
    visible_by_modifier_ = payload["visible_by_modifier"].asBool();
    spdlog::info("WayBar Shown: {}", visible_by_modifier_);
    for (auto& bar : waybar::Client::inst()->bars) {
      bar->setVisible(visible_by_modifier_);
    }
  }
}

void Hide::worker() {
  thread_ = [this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Hide: {}", e.what());
    }
  };
}

auto Hide::update() -> void {
}
}  // namespace waybar::modules::sway
