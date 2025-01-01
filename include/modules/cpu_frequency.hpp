#pragma once

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class CpuFrequency : public ALabel {
 public:
  CpuFrequency(const std::string&, const Json::Value&);
  virtual ~CpuFrequency() = default;
  auto update() -> void override;

  // This is a static member because it is also used by the cpu module.
  static std::tuple<float, float, float> getCpuFrequency();

 private:
  static std::vector<float> parseCpuFrequencies();

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
