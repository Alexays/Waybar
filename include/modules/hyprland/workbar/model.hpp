#pragma once

#include <string>
#include <vector>

namespace waybar::modules::hyprland::workbar {

struct WorkspaceState {
    int id;

    bool active = false;
    bool visible = false;

    std::string monitor;

    int windows = 0;
};

using WorkspaceList = std::vector<WorkspaceState>;

}  // namespace waybar::modules::hyprland::workbar
