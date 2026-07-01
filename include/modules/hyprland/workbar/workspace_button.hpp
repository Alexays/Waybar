#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/image.h>

#include "modules/hyprland/workbar/model.hpp"
#include "modules/hyprland/workbar/window_icon.hpp"
#include "modules/hyprland/workbar/workspace_number.hpp"

namespace waybar::modules::hyprland::workbar {

class WorkspaceButton : public Gtk::Box {
 public:
  WorkspaceButton(const WorkspaceState& workspace);
  int id() const;

  void setWorkspace(const WorkspaceState& workspace);

 private:
  
  Gtk::Box box_;
  Gtk::Box icons_;

  WorkspaceNumber number_;

  std::unordered_map<std::string, std::unique_ptr<WindowIcon>> window_icons_;
};



}  // namespace waybar::modules::hyprland