#include "modules/sway/bar.hpp"

#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

#include "bar.hpp"
#include "modules/sway/ipc/ipc.hpp"

namespace waybar::modules::sway {

BarIpcClient::BarIpcClient(waybar::Bar& bar) : bar_{bar} {
  {
    sigc::connection handle =
        ipc_.signal_cmd.connect(sigc::mem_fun(*this, &BarIpcClient::onInitialConfig));
    ipc_.sendCmd(IPC_GET_BAR_CONFIG, bar_.bar_id);

    handle.disconnect();
  }

  signal_config_.connect(sigc::mem_fun(*this, &BarIpcClient::onConfigUpdate));
  signal_visible_.connect(sigc::mem_fun(*this, &BarIpcClient::onVisibilityUpdate));

  ipc_.subscribe(R"(["bar_state_update", "barconfig_update"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &BarIpcClient::onIpcEvent));
  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("BarIpcClient::handleEvent {}", e.what());
    }
  });
}

struct swaybar_config parseConfig(const Json::Value& payload) {
  swaybar_config conf;
  if (auto id = payload["id"]; id.isString()) {
    conf.id = id.asString();
  }
  if (auto mode = payload["mode"]; mode.isString()) {
    conf.mode = mode.asString();
  }
  if (auto hs = payload["hidden_state"]; hs.isString()) {
    conf.hidden_state = hs.asString();
  }
  return conf;
}

void BarIpcClient::onInitialConfig(const struct Ipc::ipc_response& res) {
  try {
    auto payload = parser_.parse(res.payload);
    auto config = parseConfig(payload);
    onConfigUpdate(config);
  } catch (const std::exception& e) {
    spdlog::error("BarIpcClient::onInitialConfig {}", e.what());
  }
}

void BarIpcClient::onIpcEvent(const struct Ipc::ipc_response& res) {
  try {
    auto payload = parser_.parse(res.payload);
    if (auto id = payload["id"]; id.isString() && id.asString() != bar_.bar_id) {
      spdlog::trace("swaybar ipc: ignore event for {}", id.asString());
      return;
    }
    if (payload.isMember("visible_by_modifier")) {
      // visibility change for hidden bar
      signal_visible_(payload["visible_by_modifier"].asBool());
    } else {
      // configuration update
      auto config = parseConfig(payload);
      signal_config_(config);
    }
  } catch (const std::exception& e) {
    spdlog::error("BarIpcClient::onEvent {}", e.what());
  }
}

void BarIpcClient::onConfigUpdate(const swaybar_config& config) {
  spdlog::info("config update for {}: id {}, mode {}, hidden_state {}",
               bar_.bar_id,
               config.id,
               config.mode,
               config.hidden_state);
  bar_config_ = config;
  update();
}

void BarIpcClient::onVisibilityUpdate(bool visible_by_modifier) {
  spdlog::debug("visiblity update for {}: {}", bar_.bar_id, visible_by_modifier);
  visible_by_modifier_ = visible_by_modifier;
  update();
}

void BarIpcClient::update() {
  bool visible = visible_by_modifier_;
  if (bar_config_.mode == "invisible") {
    visible = false;
  } else if (bar_config_.mode != "hide" || bar_config_.hidden_state != "hide") {
    visible = true;
  }
  bar_.setMode(visible ? bar_config_.mode : Bar::MODE_INVISIBLE);
}

}  // namespace waybar::modules::sway
