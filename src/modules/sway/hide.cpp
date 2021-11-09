#include "modules/sway/hide.hpp"
#include <spdlog/spdlog.h>
#include "client.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

namespace waybar::modules::sway {

Hide::Hide(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "hide", id, "{}", 0, true), bar_(bar), windowId_(-1) {
  ipc_.subscribe(R"(["bar_state_update","barconfig_update"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Hide::onEvent));

  // override mode to "hide"
  auto &bar_local = const_cast<Bar &>(bar_);
  bar_local.config["mode"] = "hide";
  bar_local.setExclusive(false);

  if (config_["hide-on-startup"].asBool()) {
    spdlog::debug("sway/hide: Hiding on startup enabled!");
    bar_local.setHiddenClass(true);
    bar_local.moveToConfiguredLayer();
  } else {
    bar_local.moveToTopLayer();
  }

  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Hide: {}", e.what());
    }
  });
}

void Hide::onEvent(const struct Ipc::ipc_response& res) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto payload = parser_.parse(res.payload);
  auto &bar = const_cast<Bar &>(bar_);

  if (payload.isMember("mode")) {
    auto mode = payload["mode"].asString();
    if (mode == "hide") {
      // Hide the bars when configuring the "hide" bar
      spdlog::debug("sway/hide: hiding bar(s)");
      bar.setVisible(false);
      bar.setExclusive(false);
    } else if (mode == "dock") { // enables toggling the bar using killall -USR2 waybar
      spdlog::debug("sway/hide: showing bar(s)");
      bar.setVisible(true);
      bar.setExclusive(true);
    }
    return;
  }

  if (payload.isMember("visible_by_modifier")) {
    visible_by_modifier_ = payload["visible_by_modifier"].asBool();
    spdlog::debug("sway/hide: visible by modifier: {}", visible_by_modifier_);

    if (visible_by_modifier_) {
      bar.setHiddenClass(false);
      bar.setBottomLayerClass(false);
      bar.moveToTopLayer();
      return;
    }

    bool hide_to_bottom_layer_ = config_["hide-to-bottom-layer"].asBool();
    if (hide_to_bottom_layer_) {
      spdlog::debug("sway/hide: Moving bar to bottom layer instead of hiding.");
      bar.setBottomLayerClass(true);
      bar.moveToBottomLayer();
      return;
    }

    bar.setBottomLayerClass(false);
    bar.setHiddenClass(true);
    bar.moveToConfiguredLayer();
  }
}

auto Hide::update() -> void {
}

}  // namespace waybar::modules::sway
