#pragma once

#include <string>

namespace waybar::modules::hyprland::workbar {

struct DragState {
  bool dragging = false;
  std::string window_address;
  int source_workspace = -1;
};

extern DragState drag_state;

}  // namespace waybar::modules::hyprland::workbar