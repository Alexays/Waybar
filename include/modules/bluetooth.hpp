#pragma once

#include <fmt/format.h>
#include "ALabel.hpp"

namespace waybar::modules {

class Bluetooth : public ALabel {
 public:
  Bluetooth(const std::string&, const Json::Value&);
  ~Bluetooth() = default;
  auto update() -> void;

 private:
  ;
};

}  // namespace waybar::modules
