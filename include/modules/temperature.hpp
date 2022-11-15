#pragma once

#include <fmt/format.h>

#include <fstream>

#include "AButton.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Temperature : public AButton {
 public:
  Temperature(const std::string&, const Json::Value&);
  ~Temperature() = default;
  auto update() -> void;

 private:
  float getTemperature();
  bool isCritical(uint16_t);

  std::string file_path_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
