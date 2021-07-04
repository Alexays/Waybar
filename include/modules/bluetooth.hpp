#pragma once

#include "AButton.hpp"
#include "util/rfkill.hpp"

namespace waybar::modules {

class Bluetooth : public AButton {
 public:
  Bluetooth(const std::string&, const Json::Value&);
  ~Bluetooth() = default;
  auto update() -> void;

 private:
  util::Rfkill rfkill_;
};

}  // namespace waybar::modules
