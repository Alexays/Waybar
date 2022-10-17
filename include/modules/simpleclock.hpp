#pragma once

#include <fmt/chrono.h>

#include "AButton.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Clock : public AButton {
 public:
  Clock(const std::string&, const Json::Value&);
  ~Clock() = default;
  auto update() -> void;

 private:
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
