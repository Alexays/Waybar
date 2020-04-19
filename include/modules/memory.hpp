#pragma once

#include <fstream>
#include <unordered_map>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Memory : public ALabel {
 public:
  Memory(const std::string&, const Json::Value&);
  ~Memory() = default;
  auto update(std::string format, waybar::args &args) -> void override;

 private:
  static inline const std::string data_dir_ = "/proc/meminfo";

  std::unordered_map<std::string, unsigned long> parseMeminfo() const;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
