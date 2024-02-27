#pragma once

#include "ALabel.hpp"
#include "util/backlight_backend.hpp"

namespace waybar::modules {

class Backlight final : public ALabel {
 public:
  Backlight(const std::string &, const Json::Value &);
  virtual ~Backlight() = default;
  auto update() -> void override;

 private:
  const std::string preferred_device_;
  std::string previous_format_;
  util::BacklightBackend backend;

  bool handleScroll(double dx, double dy) override;
};

}  // namespace waybar::modules
