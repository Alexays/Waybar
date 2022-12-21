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

class Load : public AButton {
 public:
  Load(const std::string&, const Json::Value&);
  ~Load() = default;
  auto update() -> void;

 private:
  std::tuple<double, double, double> getLoad();

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
