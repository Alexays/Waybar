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

class CpuUsage : public ALabel {
 public:
  CpuUsage(const std::string&, const Json::Value&);
  virtual ~CpuUsage() = default;
  auto update() -> void override;

  // This is a static member because it is also used by the cpu module.
  static std::tuple<std::vector<uint16_t>, std::string> getCpuUsage(
      std::vector<std::tuple<size_t, size_t>>&);

 private:
  static std::vector<std::tuple<size_t, size_t>> parseCpuinfo();

  std::vector<std::tuple<size_t, size_t>> prev_times_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
