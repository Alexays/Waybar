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

  uint8_t getPercentage() const;
  uint16_t getTotal() const;
  uint16_t getUsed() const;
  uint16_t getAvailable() const;
  const std::string getTooltipText() const;

  unsigned long memtotal_;
  unsigned long memfree_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
