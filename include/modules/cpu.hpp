#pragma once

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Cpu final : public ALabel {
 public:
  Cpu(const std::string&, const Json::Value&);
  virtual ~Cpu() = default;
  auto update() -> void override;

 private:
  std::vector<std::tuple<size_t, size_t>> prev_times_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
