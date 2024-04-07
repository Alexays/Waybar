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

namespace wabar::modules {

class Cpu : public ALabel {
 public:
  Cpu(const std::string&, const Json::Value&);
  virtual ~Cpu() = default;
  auto update() -> void override;

 private:
  std::vector<std::tuple<size_t, size_t>> prev_times_;

  util::SleeperThread thread_;
};

}  // namespace wabar::modules
