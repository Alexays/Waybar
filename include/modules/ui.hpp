#pragma once

#include "AModule.hpp"

namespace waybar::modules {

class UI final : public AModule {
 public:
  UI(const std::string&, const std::string&, const Json::Value&);
  virtual ~UI() = default;
};

}  // namespace waybar::modules
