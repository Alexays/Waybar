#pragma once

#include <fmt/format.h>
#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules::sway {

class Mode : public ALabel, public sigc::trackable {
 public:
  Mode(const std::string&, const Json::Value&);
  ~Mode() = default;
  auto update() -> void;

 private:
  void onEvent(const struct Ipc::ipc_response&);
  void worker();

  std::string      mode_;
  util::JsonParser parser_;

  util::SleeperThread thread_;
  Ipc                 ipc_;
};

}  // namespace waybar::modules::sway
