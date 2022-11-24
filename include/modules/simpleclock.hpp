#pragma once

#include <fmt/chrono.h>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Clock : public ALabel {
 public:
  Clock(const std::string&, const Json::Value&);
  ~Clock() = default;
  auto update() -> void;

 private:
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
