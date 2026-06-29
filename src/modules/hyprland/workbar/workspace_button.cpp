#include "modules/hyprland/workbar/workspace_button.hpp"

namespace waybar::modules::hyprland::workbar {

WorkspaceButton::WorkspaceButton(int number)
    : box_(Gtk::ORIENTATION_HORIZONTAL),
      icons_(Gtk::ORIENTATION_HORIZONTAL) {

  number_.set_text(std::to_string(number));

  box_.pack_start(number_, Gtk::PACK_SHRINK);
  box_.pack_start(icons_, Gtk::PACK_SHRINK);

  add(box_);

  show_all();
}

}  // namespace waybar::modules::hyprland