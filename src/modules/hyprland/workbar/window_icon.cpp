#include <gtkmm/icontheme.h>

#include "modules/hyprland/workbar/window_icon.hpp"

namespace waybar::modules::hyprland::workbar {

WindowIcon::WindowIcon(const WindowState& window) {
    setWindow(window);
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

    add(image_);

    show_all();
}

}  // namespace waybar::modules::hyprland::workbar