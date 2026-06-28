#pragma once

#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>

namespace waybar::modules::hyprland {

class WorkspaceButton : public Gtk::Button {
 public:
  explicit WorkspaceButton(int number);

 private:
  Gtk::Box box_;
  Gtk::Label number_;
  Gtk::Box icons_;
};

}  // namespace waybar::modules::hyprland