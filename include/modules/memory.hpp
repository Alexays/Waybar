#pragma once

#include <fmt/format.h>
#include <fstream>
#include "util/chrono.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Memory : public ALabel {
  public:
    Memory(const Json::Value&);
    auto update() -> void;
  private:
    unsigned long memtotal_;
    unsigned long memfree_;
    void parseMeminfo();
    waybar::util::SleeperThread thread_;
};

}
