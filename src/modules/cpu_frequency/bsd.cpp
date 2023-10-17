#include <spdlog/spdlog.h>

#include <cmath>  // NAN

#include "modules/cpu_frequency.hpp"

std::vector<float> waybar::modules::CpuFrequency::parseCpuFrequencies() {
  static std::vector<float> frequencies;
  if (frequencies.empty()) {
    spdlog::warn(
        "cpu/bsd: parseCpuFrequencies is not implemented, expect garbage in {*_frequency}");
    frequencies.push_back(NAN);
  }
  return frequencies;
}
