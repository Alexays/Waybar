#pragma once

#include <gtkmm/box.h>

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

        std::vector<std::unique_ptr<WorkspaceButton>> workspaces_;
};

}  // namespace waybar::modules::hyprland