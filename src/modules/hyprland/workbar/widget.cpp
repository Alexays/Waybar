#include "modules/hyprland/workbar/widget.hpp"

#include <memory>

namespace waybar::modules::hyprland::workbar {

    Widget::Widget()
        : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {

        show_all();
    }

    void Widget::clearWorkspaces() {
        for (auto& button : workspaces_) {
            remove(*button);
        }

        workspaces_.clear();
    }

    void Widget::setWorkspaces(const WorkspaceList& workspaces) {
        clearWorkspaces();

        for (const auto& ws : workspaces) {
            auto button = std::make_unique<WorkspaceButton>(ws);

            pack_start(*button, Gtk::PACK_SHRINK);

            workspaces_.push_back(std::move(button));
        }

        show_all();
    }

}  // namespace waybar::modules::hyprland