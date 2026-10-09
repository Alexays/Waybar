#pragma once

#include <fmt/chrono.h>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Clock final : public ALabel {
 public:
  Clock(const std::string&, const Json::Value&);
  virtual ~Clock() = default;
  auto doUpdate() -> void override;

 private:
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
