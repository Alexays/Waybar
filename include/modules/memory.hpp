#pragma once

#include <fmt/format.h>

#include <fstream>
#include <unordered_map>

#include "AIconLabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Memory : public AIconLabel {
 public:
  Memory(const std::string&, const Json::Value&);
  virtual ~Memory() = default;
  auto update() -> void override;

 private:
  void parseMeminfo();

  std::unordered_map<std::string, unsigned long> meminfo_;

  util::SleeperThread thread_;

  std::string unit_;
};

}  // namespace waybar::modules
