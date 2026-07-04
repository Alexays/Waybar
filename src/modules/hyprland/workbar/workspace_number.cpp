#include <cstdlib>
#include <string>

#include "modules/hyprland/workbar/workspace_number.hpp"
#include "modules/hyprland/workbar/drag_state.hpp"

namespace waybar::modules::hyprland::workbar {

int WorkspaceNumber::workspaceId() const {
    return workspace_id_;
}

WorkspaceNumber::WorkspaceNumber(int workspace_id) {
    add(label_);

    get_style_context()->add_class("workspace-number");

    add_events(
        Gdk::ENTER_NOTIFY_MASK |
        Gdk::LEAVE_NOTIFY_MASK
    );

    setWorkspace(workspace_id);
    show_all();

    drag_dest_set({ Gtk::TargetEntry("WORKBAR_WINDOW") }, Gtk::DEST_DEFAULT_ALL, Gdk::DragAction::ACTION_MOVE);

    signal_drag_data_received().connect(
        [this](const Glib::RefPtr<Gdk::DragContext>&,
            int,
            int,
            const Gtk::SelectionData&,
            guint,
            guint) {
        });

    signal_clicked().connect([this]() {
        std::string cmd =
            std::string(std::getenv("HOME")) +
            "/.config/hypr/scripts/smart-workspace " +
            std::to_string(workspace_id_);

        std::system(cmd.c_str());
    });
}

void WorkspaceNumber::setWorkspace(int workspace_id) {
    workspace_id_ = workspace_id;
    label_.set_text(std::to_string(workspace_id));
}

bool WorkspaceNumber::on_enter_notify_event(GdkEventCrossing* event) {

    return Gtk::Button::on_enter_notify_event(event);
}

bool WorkspaceNumber::on_leave_notify_event(GdkEventCrossing* event) {

    return Gtk::Button::on_leave_notify_event(event);
}

}  // namespace waybar::modules::hyprland::workbar