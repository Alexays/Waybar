#pragma once

#include "ASlider.hpp"
#include "util/backlight_backend.hpp"

namespace waybar::modules {

class BacklightSlider : public ASlider {
 public:
  BacklightSlider(const std::string&, const Json::Value&);
  virtual ~BacklightSlider() = default;

  void update() override;
  void onValueChanged() override;

 private:
  std::string preferred_device_;
  util::BacklightBackend backend;
};

}  // namespace waybar::modules