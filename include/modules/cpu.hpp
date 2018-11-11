#pragma once

#include <fmt/format.h>
#include <fstream>
#include <vector>
#include <numeric>
#include "util/chrono.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Cpu : public ALabel {
  public:
    Cpu(const Json::Value&);
    auto update() -> void;
  private:
    static inline const std::string data_dir_ = "/proc/stat";
    std::vector< std::tuple<size_t, size_t> > parseCpuinfo();
    std::vector< std::tuple<size_t, size_t> > prevTimes_;
    waybar::util::SleeperThread thread_;
};

}
