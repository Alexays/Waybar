#pragma once

#include <fmt/format.h>
#include <tuple>
#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules::sway {

class Hide : public ALabel, public sigc::trackable {
 public:
  Hide(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Hide() = default;
  auto update() -> void;

 private:
  void onEvent(const struct Ipc::ipc_response&);
  void worker();

  const Bar&       bar_;
  std::string      window_;
  int              windowId_;
  util::JsonParser parser_;

  util::SleeperThread thread_;
  Ipc                 ipc_;
};

}  // namespace waybar::modules::sway
