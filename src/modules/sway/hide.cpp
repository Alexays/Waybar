#include "modules/sway/hide.hpp"
#include <spdlog/spdlog.h>
#include "client.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

namespace waybar::modules::sway {

Hide::Hide(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "hide", id, "{}", 0, true), bar_(bar), windowId_(-1) {
  ipc_.subscribe(R"(["bar_state_update","barconfig_update"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Hide::onEvent));
  bar_.window.get_style_context()->add_class("hidden");
  // Launch worker
  worker();
}

void Hide::onEvent(const struct Ipc::ipc_response& res) {
  auto client = waybar::Client::inst();

  auto payload = parser_.parse(res.payload);
  if (payload.isMember("mode")) {
    // barconfig_update: get mode
    current_mode_ = payload["mode"].asString();
  } else if (payload.isMember("visible_by_modifier")) {
    // bar_state_update: get visible_by_modifier
    visible_by_modifier_ = payload["visible_by_modifier"].asBool();
  }
  if (current_mode_ == "invisible"
      || (current_mode_ == "hide" && ! visible_by_modifier_)) {
    bar_.window.get_style_context()->add_class("hidden");
    zwlr_layer_surface_v1_set_layer(bar_.layer_surface,
                                    ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM);
  } else {
    // TODO: support "bar mode overlay"
    bar_.window.get_style_context()->remove_class("hidden");
    zwlr_layer_surface_v1_set_layer(bar_.layer_surface,
                                    ZWLR_LAYER_SHELL_V1_LAYER_TOP
                                    | ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY);
  }
  wl_surface_commit(bar_.surface);
  wl_display_roundtrip(client->wl_display);
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
