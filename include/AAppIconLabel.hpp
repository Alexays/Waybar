#pragma once

#include <gtkmm/box.h>
#include <gtkmm/image.h>

#include "AIconLabel.hpp"

namespace waybar {

class AAppIconLabel : public AIconLabel {
 public:
  AAppIconLabel(const Json::Value &config, const std::string &name, const std::string &id,
                const std::string &format, uint16_t interval = 0, bool ellipsize = false,
                bool enable_click = false, bool enable_scroll = false);
  virtual ~AAppIconLabel() = default;
  auto update() -> void override;

 protected:
  void updateAppIconName(const std::string &app_identifier,
                         const std::string &alternative_app_identifier);
  void updateAppIcon();
  unsigned app_icon_size_{24};
  bool update_app_icon_{true};
  std::string app_icon_name_;
};

}  // namespace waybar
