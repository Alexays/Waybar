#pragma once

#include <gtkmm/box.h>

#include "modules/hyprland/workspace_button.hpp"

#include <memory>
#include <vector>

namespace waybar::modules::hyprland {

class WorkbarWidget : public Gtk::Box {
    public:
        WorkbarWidget();

        void setWorkspaces(const std::vector<int>& workspaces);

    private:
        void clearWorkspaces();

        std::vector<std::unique_ptr<WorkspaceButton>> workspaces_;
};

}  // namespace waybar::modules::hyprland