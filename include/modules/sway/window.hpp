#pragma once

#include <fmt/format.h>
#include <tuple>
#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules::sway {

class Window : public ALabel {
 public:
  Window(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Window() = default;
  auto update() -> void;

 private:
  void                         onEvent(const struct Ipc::ipc_response);
  void                         onCmd(const struct Ipc::ipc_response);
  void                         worker();
  std::tuple<int, std::string> getFocusedNode(Json::Value nodes);
  void                         getFocusedWindow();

  const Bar&                  bar_;
  waybar::util::SleeperThread thread_;
  Ipc                         ipc_;
  std::string                 window_;
  int                         windowId_;
};

}  // namespace waybar::modules::sway
