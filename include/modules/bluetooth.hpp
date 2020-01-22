#pragma once

#include <fmt/format.h>
#include "ALabel.hpp"

#include <fmt/chrono.h>
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Bluetooth : public ALabel {
 public:
  Bluetooth(const std::string&, const Json::Value&);
  ~Bluetooth() = default;
  auto update() -> void;

 private:
  std::string                   status_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
