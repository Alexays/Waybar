#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <gdk/gdk.h>
#include <cstdlib>

#include <gtkmm/icontheme.h>

#include "modules/hyprland/workbar/window_icon.hpp"
#include "modules/hyprland/workbar/drag_state.hpp"
#include "modules/hyprland/workbar/widget.hpp"

using WorkbarWidget = waybar::modules::hyprland::workbar::Widget;

namespace waybar::modules::hyprland::workbar {

WindowIcon::WindowIcon(const WindowState& window) {

    add(image_);

    add_events(
        Gdk::BUTTON_PRESS_MASK |
        Gdk::BUTTON_RELEASE_MASK |
        Gdk::POINTER_MOTION_MASK
    );

    // Existing signal_clicked()
    // Existing signal_button_press_event()
    // ...

    setWindow(window);

    show_all();
}

void WindowIcon::setWindow(const WindowState& window) {
    auto theme = Gtk::IconTheme::get_default();

    if (theme->has_icon(window.class_name)) {
        image_.set_from_icon_name(window.class_name,
                                Gtk::ICON_SIZE_MENU);
    } else {
        image_.set_from_icon_name("application-x-executable",
                                Gtk::ICON_SIZE_MENU);
    }

    window_ = window;

    show_all();

    signal_clicked().connect([this]() {

        std::string cmd =
            std::string(std::getenv("HOME")) +
            "/.config/hypr/scripts/smart-workspace " +
            std::to_string(window_.workspace_id);

        std::system(cmd.c_str());

        std::this_thread::sleep_for(std::chrono::milliseconds(40));

        cmd = "hyprctl dispatch focuswindow address:" + window_.address;

        std::system(cmd.c_str());
    });

    std::vector<Gtk::TargetEntry> targets = {
        Gtk::TargetEntry("WORKBAR_WINDOW")
    };

    add_events(Gdk::BUTTON_PRESS_MASK);

    
}


bool WindowIcon::on_button_press_event(GdkEventButton* event) {

    if (event->button == 1) {
        left_pressed_ = true;

        press_x_ = event->x;
        press_y_ = event->y;
        dragging_ = false;
    }

    return Gtk::Button::on_button_press_event(event);
}

bool WindowIcon::on_motion_notify_event(GdkEventMotion* event) {
    if (!left_pressed_) {
        return Gtk::Button::on_motion_notify_event(event);
    }

    if (!dragging_) {

        double dx = event->x - press_x_;
        double dy = event->y - press_y_;

        if (dx * dx + dy * dy > 36) {
            dragging_ = true;

            auto* widget = WorkbarWidget::instance();

            if (widget) {
                widget->beginDrag(window_);
            }
        }
    }

    // <-- ADD THIS NEW BLOCK
    if (dragging_) {

        auto* widget = WorkbarWidget::instance();

        if (widget) {
            widget->updateDrag(event->x_root, event->y_root);
        }
    }

    return Gtk::Button::on_motion_notify_event(event);
}

bool WindowIcon::on_button_release_event(GdkEventButton* event) {

    left_pressed_ = false;

    if (dragging_) {

        dragging_ = false;

        auto* widget = WorkbarWidget::instance();
        if (widget) {
            widget->endDrag();
        }

    } else if (event->button == 1) {

        std::string cmd =
            std::string(std::getenv("HOME")) +
            "/.config/hypr/scripts/smart-workspace " +
            std::to_string(window_.workspace_id);

        std::system(cmd.c_str());

        std::this_thread::sleep_for(std::chrono::milliseconds(40));

        cmd = "hyprctl dispatch focuswindow address:" + window_.address;

        std::system(cmd.c_str());

    }

    return true;
}

}  // namespace waybar::modules::hyprland::workbar