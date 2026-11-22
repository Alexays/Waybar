#pragma once

#include <fmt/format.h>

#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Cpu : public ALabel {
 public:
  Cpu(const std::string&, const Json::Value&);
  auto update() -> void override;

 private:
  std::vector<std::tuple<std::size_t, std::size_t>> prev_times_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
