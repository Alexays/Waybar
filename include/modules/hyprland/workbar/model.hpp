#pragma once

#include <string>
#include <vector>

namespace waybar::modules::hyprland::workbar {

struct WindowState {
    std::string address;
    std::string class_name;
    std::string title;
};

struct WorkspaceState {
    int id;

    bool active = false;
    bool visible = false;

    std::string monitor;

    std::vector<WindowState> windows;
};

using WorkspaceList = std::vector<WorkspaceState>;

}  // namespace waybar::modules::hyprland::workbar
