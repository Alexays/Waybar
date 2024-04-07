#pragma once

#include <fmt/format.h>

#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/json.hpp"

namespace wabar::modules::hyprland {

class Submap : public wabar::ALabel, public EventHandler {
 public:
  Submap(const std::string&, const wabar::Bar&, const Json::Value&);
  ~Submap() override;

  auto update() -> void override;

 private:
  auto parseConfig(const Json::Value&) -> void;
  void onEvent(const std::string& ev) override;

  std::mutex mutex_;
  const Bar& bar_;
  util::JsonParser parser_;
  std::string submap_;
  bool always_on_ = false;
  std::string default_submap_ = "Default";
};

}  // namespace wabar::modules::hyprland
