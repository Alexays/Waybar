#pragma once

#include <gtkmm/box.h>

namespace waybar::util::gtk {

static void move_widget(Gtk::Box& box, Gtk::Widget& wg, int pos) {
  if (pos <= 0) return box.reorder_child_at_start(wg);

  // Get wg position
  int wg_pos{0};
  for (const auto& w : box.get_children()) {
    if (w == &wg) break;
    ++wg_pos;
  }

  if (pos == wg_pos) return;

  box.reorder_child_after(wg, *(box.get_children().at((pos < wg_pos) ? pos - 1 : pos)));
}

}  // namespace waybar::util::gtk
