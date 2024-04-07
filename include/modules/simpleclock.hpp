#pragma once

#include <fmt/chrono.h>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace wabar::modules {

class Clock : public ALabel {
 public:
  Clock(const std::string&, const Json::Value&);
  virtual ~Clock() = default;
  auto update() -> void override;

 private:
  util::SleeperThread thread_;
};

}  // namespace wabar::modules
