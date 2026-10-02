#pragma once

#include <gtkmm/button.h>
#include <gtkmm/image.h>

#include "modules/hyprland/workbar/model.hpp"

namespace waybar::modules::hyprland::workbar {

class WindowIcon : public Gtk::Button {
 public:
  explicit WindowIcon(const WindowState& window);

  void setWindow(const WindowState& window);

 protected:
  bool on_button_press_event(GdkEventButton* event) override;
  bool on_motion_notify_event(GdkEventMotion* event) override;
  bool on_button_release_event(GdkEventButton* event) override;

 private:
  void focusWindow();
  Gtk::Image image_;
  WindowState window_;

  bool left_pressed_ = false;
  bool dragging_ = false;
  double press_x_ = 0;
  double press_y_ = 0;
};

}  // namespace waybar::modules::hyprland::workbar