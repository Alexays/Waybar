#pragma once

#include <fmt/format.h>
#include "util/chrono.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Custom : public ALabel {
  public:
    Custom(const std::string&, Json::Value);
    auto update() -> void;
  private:
    const std::string& name_;
    waybar::util::SleeperThread thread_;
};

}
