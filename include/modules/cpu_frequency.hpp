#pragma once

#include <fmt/format.h>

#include <cstdint>
#include <fstream>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "AButton.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class CpuFrequency : public AButton {
 public:
  CpuFrequency(const std::string&, const Json::Value&);
  ~CpuFrequency() = default;
  auto update() -> void;

 private:
  std::tuple<float, float, float> getCpuFrequency();
  std::vector<float> parseCpuFrequencies();

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
