#pragma once

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Disk final : public ALabel {
 public:
  Disk(const std::string&, const Json::Value&);
  virtual ~Disk() = default;
  auto update() -> void override;

 private:
  util::SleeperThread thread_;
  std::string path_;
  std::string unit_;

  float calc_specific_divisor(const std::string divisor);
};

}  // namespace waybar::modules
