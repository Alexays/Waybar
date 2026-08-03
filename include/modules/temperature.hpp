#pragma once

#include <fmt/format.h>

#include <fstream>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

enum SensorType { TEMPERATURE, FAN, POWER };

class Temperature : public ALabel {
 public:
  Temperature(const std::string&, const Json::Value&);
  virtual ~Temperature() = default;
  auto update() -> void override;
  void suspend() override;
  void resume() override;

 private:
  float getReadings();
  bool isCritical(uint16_t);
  bool isWarning(uint16_t);

  std::string file_path_;
  SensorType sensor_type_{TEMPERATURE};
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
