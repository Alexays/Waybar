#pragma once

#include <fmt/format.h>

#include <cstdint>
#include <fstream>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "AGraph.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class CpuGraph : public AGraph {
 public:
  CpuGraph(const std::string&, const Json::Value&);
  virtual ~CpuGraph() = default;
  auto update() -> void override;

 private:
  static constexpr const char *MODERATE_CLASS = "cpu-moderate";
  static constexpr const char *HIGH_CLASS = "cpu-high";
  static constexpr const char *INTENSIVE_CLASS = "cpu-intensive";

  std::vector<std::tuple<size_t, size_t>> prev_times_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
