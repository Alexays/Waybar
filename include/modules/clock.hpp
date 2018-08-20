#pragma once

#include <fmt/format.h>
#include "fmt/time.h"
#include "util/chrono.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Clock : public ALabel {
  public:
    Clock(const Json::Value&);
    auto update() -> void;
  private:
    waybar::util::SleeperThread thread_;
};

}
