#pragma once

#include <gtkmm/image.h>
#include <gtkmm/button.h>

#include "modules/hyprland/workbar/model.hpp"

namespace waybar::modules::hyprland::workbar {

class WindowIcon : public Gtk::Button {
 public:
  explicit WindowIcon(const WindowState& window);

  void setWindow(const WindowState& window);

 private:
    Gtk::Image image_;
};

}  // namespace waybar::modules::hyprland::workbar