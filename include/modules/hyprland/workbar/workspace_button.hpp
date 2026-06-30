#pragma once

#include <memory>
#include <vector>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/image.h>

#include "modules/hyprland/workbar/model.hpp"
#include "modules/hyprland/workbar/window_icon.hpp"

namespace waybar::modules::hyprland::workbar {

class WorkspaceButton : public Gtk::Button {
 public:
  WorkspaceButton(const WorkspaceState& workspace);

  void setWorkspace(const WorkspaceState& workspace);

 private:
  
  Gtk::Box box_;
  Gtk::Label number_;
  Gtk::Box icons_;

  std::vector<std::unique_ptr<WindowIcon>> window_icons_;
};



}  // namespace waybar::modules::hyprland