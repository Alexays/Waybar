#pragma once

#include <fstream>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Temperature : public ALabel {
 public:
  Temperature(const std::string&, const Json::Value&);
  ~Temperature() = default;
  auto update(std::string format, ALabel::args &args) -> void override;

 private:
  uint16_t getTemperature() const;
  bool isCritical(uint16_t) const;

  std::string file_path_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
