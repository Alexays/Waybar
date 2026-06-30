#pragma once

#include <gtkmm/box.h>
#include <unordered_map>

#include "modules/hyprland/workbar/model.hpp"
#include "modules/hyprland/workbar/workspace_button.hpp"

#include <memory>
#include <vector>

namespace waybar::modules::hyprland::workbar {

class Widget : public Gtk::Box {
    public:
        Widget();

        void setWorkspaces(const WorkspaceList& workspaces);

    private:
        void clearWorkspaces();

        std::unordered_map<int, std::unique_ptr<WorkspaceButton>> workspaces_;
};

}  // namespace waybar::modules::hyprland