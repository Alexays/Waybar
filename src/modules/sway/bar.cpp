#include "modules/sway/bar.hpp"

#include <spdlog/spdlog.h>

#include <sstream>
#include <stdexcept>

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

  Json::Value subscribe_events{Json::arrayValue};
  subscribe_events.append("bar_state_update");
  subscribe_events.append("barconfig_update");

  bool has_mode = isModuleEnabled("sway/mode");
  bool has_workspaces = isModuleEnabled("sway/workspaces");

  if (has_mode) {
    subscribe_events.append("mode");
  }
  if (has_workspaces) {
    subscribe_events.append("workspace");
  }
  if (has_mode || has_workspaces) {
    subscribe_events.append("binding");
  }

  modifier_reset_ = bar.config.get("modifier-reset", "press").asString();

  signal_config_.connect(sigc::mem_fun(*this, &BarIpcClient::onConfigUpdate));
  signal_visible_.connect(sigc::mem_fun(*this, &BarIpcClient::onVisibilityUpdate));
  signal_urgency_.connect(sigc::mem_fun(*this, &BarIpcClient::onUrgencyUpdate));
  signal_mode_.connect(sigc::mem_fun(*this, &BarIpcClient::onModeUpdate));

  // Subscribe to non bar events to determine if the modifier key press is followed by another
  // action.
  std::ostringstream oss_events;
  oss_events << subscribe_events;
  ipc_.subscribe(oss_events.str());
  ipc_.signal_event.connect(sigc::mem_fun(*this, &BarIpcClient::onIpcEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &BarIpcClient::onCmd));
  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("BarIpcClient::handleEvent {}", e.what());
    }
  });
}

bool BarIpcClient::isModuleEnabled(std::string name) {
  for (const auto& section : {"modules-left", "modules-center", "modules-right"}) {
    if (const auto& modules = bar_.config.get(section, {}); modules.isArray()) {
      for (const auto& module : modules) {
        if (module.asString().rfind(name, 0) == 0) {
          return true;
        }
      }
    }
  }
  return false;
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
  auto payload = parser_.parse(res.payload);
  if (auto success = payload.get("success", true); !success.asBool()) {
    auto err = payload.get("error", "Unknown error");
    throw std::runtime_error(err.asString());
  }
  auto config = parseConfig(payload);
  onConfigUpdate(config);
}

void BarIpcClient::onIpcEvent(const struct Ipc::ipc_response& res) {
  try {
    auto payload = parser_.parse(res.payload);
    switch (res.type) {
      case IPC_EVENT_WORKSPACE:
        if (payload.isMember("change")) {
          // only check and send signal if the workspace update reason was because of a urgent
          // change
          if (payload["change"] == "urgent") {
            auto urgent = payload["current"]["urgent"];
            if (urgent.asBool()) {
              // Event for a new urgency, update the visibly
              signal_urgency_(true);
            } else if (!urgent.asBool() && visible_by_urgency_) {
              // Event clearing an urgency, bar is visible, check if another workspace still has
              // the urgency hint set
              ipc_.sendCmd(IPC_GET_WORKSPACES);
            }
          }
          modifier_no_action_ = false;
        }
        break;
      case IPC_EVENT_MODE:
        if (payload.isMember("change")) {
          signal_mode_(payload["change"] != "default");
          modifier_no_action_ = false;
        }
        break;
      case IPC_EVENT_BINDING:
        modifier_no_action_ = false;
        break;
      case IPC_EVENT_BAR_STATE_UPDATE:
      case IPC_EVENT_BARCONFIG_UPDATE:
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
          signal_config_(std::move(config));
        }
        break;
    }
  } catch (const std::exception& e) {
    spdlog::error("BarIpcClient::onEvent {}", e.what());
  }
}

void BarIpcClient::onCmd(const struct Ipc::ipc_response& res) {
  if (res.type == IPC_GET_WORKSPACES) {
    try {
      auto payload = parser_.parse(res.payload);
      for (auto& ws : payload) {
        if (ws["urgent"].asBool()) {
          spdlog::debug("Found workspace {} with urgency set. Stopping search.", ws["name"]);
          // Found one workspace with urgency set, signal bar visibility
          signal_urgency_(true);
          return;
        }
      }
      // Command to get workspaces was sent after a change in workspaces was based on "urgent",
      // if no workspace has this flag set to true, all flags must be cleared.
      signal_urgency_(false);
    } catch (const std::exception& e) {
      spdlog::error("Bar: {}", e.what());
    }
  }
}

void BarIpcClient::onConfigUpdate(const swaybar_config& config) {
  spdlog::info("config update for {}: id {}, mode {}, hidden_state {}", bar_.bar_id, config.id,
               config.mode, config.hidden_state);
  bar_config_ = config;
  update();
}

void BarIpcClient::onModeUpdate(bool visible_by_mode) {
  spdlog::debug("mode update for {}: {}", bar_.bar_id, visible_by_mode);
  visible_by_mode_ = visible_by_mode;
  update();
}

void BarIpcClient::onVisibilityUpdate(bool visible_by_modifier) {
  spdlog::debug("visibility update for {}: {}", bar_.bar_id, visible_by_modifier);
  visible_by_modifier_ = visible_by_modifier;
  if (visible_by_modifier) {
    modifier_no_action_ = true;
  }

  // Clear on either press or release depending on bar_.bar_config_.action value.
  // For the check on release, make sure that the modifier key was not used for another action.
  if (((modifier_reset_ == "press" && visible_by_modifier_) ||
       (modifier_reset_ == "release" && !visible_by_modifier_ && modifier_no_action_))) {
    // Clear the flags to hide the bar.
    visible_by_urgency_ = false;
    visible_by_mode_ = false;
  }

  update();
}

void BarIpcClient::onUrgencyUpdate(bool visible_by_urgency) {
  spdlog::debug("urgency update for {}: {}", bar_.bar_id, visible_by_urgency);
  visible_by_urgency_ = visible_by_urgency;
  update();
}

void BarIpcClient::update() {
  bool visible = visible_by_modifier_ || visible_by_mode_ || visible_by_urgency_;
  if (bar_config_.mode == "invisible") {
    visible = false;
  } else if (bar_config_.mode != "hide" || bar_config_.hidden_state != "hide") {
    visible = true;
  }
  bar_.setMode(visible ? bar_config_.mode : Bar::MODE_INVISIBLE);
}

}  // namespace waybar::modules::sway
