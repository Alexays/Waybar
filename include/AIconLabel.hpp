#pragma once

#include <gtkmm/box.h>
#include <gtkmm/image.h>

#include "ALabel.hpp"

namespace waybar {

class AIconLabel : public ALabel {
 public:
  AIconLabel(const Json::Value& config, const std::string& name, const std::string& id,
             const std::string& format, uint16_t interval = 0, bool ellipsize = false,
             bool enable_click = false, bool enable_scroll = false);
  virtual ~AIconLabel() = default;
  auto update() -> void override;
  static std::tuple<std::string, std::string> extractIcon(const std::string& input);

 protected:
  Gtk::Image image_;
  Gtk::Box box_;
  unsigned app_icon_size_{24};

  bool label_contains_icon{false};
  // Only a label written by the user may embed an icon directive: foreign text such as a
  // window title must not pick a file for the bar to open.
  bool embedded_icon_enabled_{false};

  bool iconEnabled() const;
};

}  // namespace waybar
