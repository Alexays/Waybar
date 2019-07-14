#pragma once

#include <fmt/format.h>
#include <fstream>
#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Temperature : public ALabel {
 public:
  Temperature(const std::string&, const Json::Value&);
  ~Temperature() = default;
  auto update() -> void;

 private:
  uint16_t getTemperature() const;
  bool     isCritical() const;

  const std::string              getFormat() const override;
  const std::vector<std::string> getClasses() const override;

  uint16_t getCelcius() const;
  uint16_t getFahrenheit() const;

  std::string         file_path_;
  uint16_t            temperature_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
