#pragma once

#include <fmt/format.h>
#include <fstream>
#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Memory : public ALabel {
 public:
  Memory(const std::string&, const Json::Value&);
  ~Memory() = default;
  auto update() -> void;

 private:
  static inline const std::string data_dir_ = "/proc/meminfo";
  void                            parseMeminfo();

  std::unordered_map<std::string, unsigned long> meminfo_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
