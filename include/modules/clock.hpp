#pragma once

#include <fmt/format.h>
#include "fmt/time.h"
#include "util/sleeper_thread.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Clock : public ALabel {
  public:
    Clock(const std::string&, const Json::Value&);
    ~Clock() = default;
    auto update() -> void;
  private:
    waybar::util::SleeperThread thread_;
};

}
