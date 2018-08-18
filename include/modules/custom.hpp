#pragma once

#include <fmt/format.h>
#include <iostream>
#include <sys/wait.h>
#include "util/chrono.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Custom : public ALabel {
  public:
    Custom(std::string, Json::Value);
    auto update() -> void;
  private:
    std::string name_;
    waybar::util::SleeperThread thread_;
};

}
