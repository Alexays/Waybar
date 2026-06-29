#pragma once

#include <vector>

namespace waybar::modules::hyprland::workbar {

struct WorkspaceState {
  int id;
};

using WorkspaceList = std::vector<WorkspaceState>;

}  // namespace waybar::modules::hyprland::workbar
