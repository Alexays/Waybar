#pragma once

#include <fmt/format.h>

#include <cstdint>
#include <fstream>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Cpu : public ALabel {
 public:
  Cpu(const std::string&, const Json::Value&);
  virtual ~Cpu() = default;
  auto update() -> void override;

 private:
  double getCpuLoad();
  std::tuple<std::vector<uint16_t>, std::string> getCpuUsage();
  std::tuple<float, float, float> getCpuFrequency();
  std::vector<std::tuple<size_t, size_t>> parseCpuinfo();
  std::vector<float> parseCpuFrequencies();

  std::vector<std::tuple<size_t, size_t>> prev_times_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
