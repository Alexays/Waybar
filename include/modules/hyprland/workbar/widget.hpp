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
        static Widget* instance();
        WorkspaceButton* workspaceAt(int x, int y);
        void setWorkspaces(const WorkspaceList& workspaces);
        void beginDrag(const WindowState& window);
        void updateDrag(double x, double y);
        void endDrag();

    private:
        void clearWorkspaces();
        static Widget* instance_;
        std::unordered_map<int, std::unique_ptr<WorkspaceButton>> workspaces_;
        bool dragging_ = false;
        WindowState dragged_window_;
        WorkspaceButton* hovered_workspace_ = nullptr;
};

}  // namespace waybar::modules::hyprland