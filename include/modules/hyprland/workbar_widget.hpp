#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>

namespace waybar::modules::hyprland {

class WorkbarWidget : public Gtk::Box {
 public:
  WorkbarWidget();

 private:
  Gtk::Label label_;
};

}  // namespace waybar::modules::hyprland