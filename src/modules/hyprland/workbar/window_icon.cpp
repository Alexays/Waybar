#include <cstdlib>
#include <thread>
#include <chrono>
#include <gdk/gdk.h>

#include <gtkmm/icontheme.h>

#include "modules/hyprland/workbar/window_icon.hpp"
#include "modules/hyprland/workbar/drag_state.hpp"
#include "modules/hyprland/workbar/widget.hpp"

using WorkbarWidget = waybar::modules::hyprland::workbar::Widget;

namespace waybar::modules::hyprland::workbar {

WindowIcon::WindowIcon(const WindowState& window) {

    add(image_);

    get_style_context()->add_class("window-icon");

    add_events(
        Gdk::BUTTON_PRESS_MASK |
        Gdk::BUTTON_RELEASE_MASK |
        Gdk::POINTER_MOTION_MASK
    );

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

    setWindow(window);

    show_all();

}

void WindowIcon::setWindow(const WindowState& window) {
    auto theme = Gtk::IconTheme::get_default();

    try {
        auto pixbuf = theme->load_icon(
            theme->has_icon(window.class_name)
                ? window.class_name
                : "application-x-executable",
            64,   // load a large icon
            Gtk::ICON_LOOKUP_FORCE_SIZE);

        auto scaled = pixbuf->scale_simple(
            16, 16,
            Gdk::INTERP_BILINEAR);

        image_.set(scaled);
            
    } catch (...) {
    }

    window_ = window;

    auto context = get_style_context();

    if (window.active) {
        context->add_class("active");
    } else {
        context->remove_class("active");
    }

    
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

    if (event->button == 2) {
        std::string cmd =
            "hyprctl dispatch closewindow address:" + window_.address;

        std::system(cmd.c_str());

        return true;
    } 

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