#pragma once

#include <fmt/format.h>
#include "ALabel.hpp"
#include "fmt/time.h"
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
