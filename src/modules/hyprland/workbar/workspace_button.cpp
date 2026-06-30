#include "modules/hyprland/workbar/workspace_button.hpp"

namespace waybar::modules::hyprland::workbar {

WorkspaceButton::WorkspaceButton(const WorkspaceState& workspace)
    : box_(Gtk::ORIENTATION_HORIZONTAL),
      icons_(Gtk::ORIENTATION_HORIZONTAL) {

    box_.pack_start(number_, Gtk::PACK_SHRINK);
    box_.pack_start(icons_, Gtk::PACK_SHRINK);

    add(box_);

    setWorkspace(workspace);

    show_all();
}

void WorkspaceButton::setWorkspace(const WorkspaceState& workspace) {

    window_icons_.clear();

    for (auto* child : icons_.get_children()) {
        icons_.remove(*child);
    }

    number_.set_text(std::to_string(workspace.id));

    auto context = get_style_context();

    context->add_class("workspace");

    if (workspace.active) {
        context->add_class("active");
    } else {
        context->remove_class("active");
    }

    if (workspace.visible) {
        context->add_class("visible");
    } else {
        context->remove_class("visible");
    }

    for (const auto& window : workspace.windows) {
        auto icon = std::make_unique<WindowIcon>(window);

        icons_.pack_start(*icon, Gtk::PACK_SHRINK);

        window_icons_.push_back(std::move(icon));
    }

    show_all();
}


}  // namespace waybar::modules::hyprland::workbar