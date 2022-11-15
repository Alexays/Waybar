#pragma once

#include <fmt/format.h>

#include <fstream>
#include <unordered_map>

#include "AButton.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Memory : public AButton {
 public:
  Memory(const std::string&, const Json::Value&);
  ~Memory() = default;
  auto update() -> void;

 private:
  void parseMeminfo();

  std::unordered_map<std::string, unsigned long> meminfo_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
