#pragma once

#include <unordered_map>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Memory final : public ALabel {
 public:
  Memory(const std::string&, const Json::Value&);
  virtual ~Memory() = default;
  auto update() -> void override;

 private:
  void parseMeminfo();

  std::unordered_map<std::string, unsigned long> meminfo_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
