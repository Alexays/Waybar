#pragma once

#include <fmt/format.h>

#include <fstream>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace wabar::modules {

class Temperature : public ALabel {
 public:
  Temperature(const std::string&, const Json::Value&);
  virtual ~Temperature() = default;
  auto update() -> void override;

 private:
  float getTemperature();
  bool isCritical(uint16_t);

  std::string file_path_;
  util::SleeperThread thread_;
};

}  // namespace wabar::modules
