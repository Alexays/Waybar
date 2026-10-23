#pragma once

#include <fmt/format.h>

#include <fstream>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Temperature final : public ALabel {
 public:
  Temperature(const std::string&, const Json::Value&);
  virtual ~Temperature() = default;

 private:
  void doUpdate() override;
  void doSuspend() override;
  void doResume() override;
  float getTemperature();
  bool isCritical(uint16_t);
  bool isWarning(uint16_t);

  std::string file_path_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
