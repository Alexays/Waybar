#pragma once
#include <string>

#include "modules/sway/ipc/client.hpp"
#include "util/SafeSignal.hpp"
#include "util/json.hpp"

namespace waybar {

class Bar;

namespace modules::sway {

/*
 * Supported subset of i3/sway IPC barconfig object
 */
struct swaybar_config {
  std::string id;
  std::string mode;
  std::string hidden_state;
  std::string position;
};

/**
 * swaybar IPC client
 */
class BarIpcClient {
 public:
  BarIpcClient(waybar::Bar& bar);

 private:
  void onInitialConfig(const struct Ipc::ipc_response& res);
  void onIpcEvent(const struct Ipc::ipc_response&);
  void onConfigUpdate(const swaybar_config& config);
  void onVisibilityUpdate(bool visible_by_modifier);
  void update();

  Bar&             bar_;
  util::JsonParser parser_;
  Ipc              ipc_;

  swaybar_config bar_config_;
  bool           visible_by_modifier_ = false;

  SafeSignal<bool>           signal_visible_;
  SafeSignal<swaybar_config> signal_config_;
};

}  // namespace modules::sway
}  // namespace waybar
