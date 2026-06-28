#include "modules/hyprland/workbar_widget.hpp"

#include <memory>

namespace waybar::modules::hyprland {

    WorkbarWidget::WorkbarWidget()
        : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {

        setWorkspaces({1, 2, 3, 4});

        show_all();
    }

    void WorkbarWidget::clearWorkspaces() {
        for (auto& button : workspaces_) {
            remove(*button);
        }

        workspaces_.clear();
    }

    void WorkbarWidget::setWorkspaces(const std::vector<int>& workspaces) {
        clearWorkspaces();

        for (int ws : workspaces) {
            auto button = std::make_unique<WorkspaceButton>(ws);

            pack_start(*button, Gtk::PACK_SHRINK);

            workspaces_.push_back(std::move(button));
        }

        show_all();
    }

}  // namespace waybar::modules::hyprland