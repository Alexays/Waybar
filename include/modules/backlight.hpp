#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ALabel.hpp"
#include "util/backlight_backend.hpp"
#include "util/json.hpp"

struct udev;
struct udev_device;

namespace waybar::modules {

class Backlight : public ALabel {
 public:
  Backlight(const std::string&, const Json::Value&);
  virtual ~Backlight() = default;
  auto update() -> void override;

  bool handleScroll(GdkEventScroll* e) override;

  const std::string preferred_device_;

  std::string previous_format_;

  util::BacklightBackend backend;
};
}  // namespace waybar::modules
