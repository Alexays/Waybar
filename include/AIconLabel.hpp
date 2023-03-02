#pragma once

#include <gtkmm/box.h>
#include <gtkmm/image.h>

#include "ALabel.hpp"

namespace waybar {

class AIconLabel : public ALabel {
 public:
  AIconLabel(const Json::Value &config, const std::string &name, const std::string &id,
             const std::string &format, uint16_t interval = 0, bool ellipsize = false,
             bool enable_click = false, bool enable_scroll = false);
  virtual ~AIconLabel() = default;
  auto update() -> void override;

 protected:
  Gtk::Image image_;
  Gtk::Box box_;

  bool iconEnabled() const;
};

}  // namespace waybar
