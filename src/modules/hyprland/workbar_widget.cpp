#include "modules/hyprland/workbar_widget.hpp"

namespace waybar::modules::hyprland {

WorkbarWidget::WorkbarWidget()
    : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {
  label_.set_text("Hello from WorkbarWidget");

  pack_start(label_, Gtk::PACK_SHRINK);

  show_all();
}

}  // namespace waybar::modules::hyprland