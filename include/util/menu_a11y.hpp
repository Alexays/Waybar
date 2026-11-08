#pragma once

#include <gtk/gtk.h>

namespace waybar::util {

// Names the items of a popover menu on the accessibility bus with their label text, as GTK3 menu
// items were: GTK 4.24 does not derive the name from a menu item's label. With show_icons, items
// show their icon beside their label, as GTK3 image menu items did. GTK resets both whenever the
// menu model updates an item, so this runs when the menu maps and after each update.
inline void update_menu_items(GtkWidget* widget, bool show_icons) {
  static const GType model_button = g_type_from_name("GtkModelButton");
  for (auto* child = gtk_widget_get_first_child(widget); child != nullptr;
       child = gtk_widget_get_next_sibling(child)) {
    if (G_TYPE_CHECK_INSTANCE_TYPE(child, model_button)) {
      bool named = false;
      // The image comes before the label, the first label is the item text, the next one its
      // accelerator
      for (auto* part = gtk_widget_get_first_child(child); part != nullptr;
           part = gtk_widget_get_next_sibling(part)) {
        if (show_icons && GTK_IS_IMAGE(part)) {
          gtk_widget_set_visible(part, TRUE);
          gtk_widget_set_hexpand(part, FALSE);
        } else if (!named && GTK_IS_LABEL(part)) {
          named = true;
          gtk_widget_set_hexpand(part, gtk_widget_get_visible(part));
          // The labelled-by relation would win over the label property
          gtk_accessible_reset_relation(GTK_ACCESSIBLE(child), GTK_ACCESSIBLE_RELATION_LABELLED_BY);
          gtk_accessible_update_property(GTK_ACCESSIBLE(child), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                         gtk_label_get_text(GTK_LABEL(part)), -1);
        }
      }
    }
    update_menu_items(child, show_icons);
  }
}

// GTK4 sets :hover on every ancestor of the widget under the pointer, and the ancestors of a
// popover menu are its module and the bar. GTK3 menus grabbed the pointer, which left them
// unhovered.
inline void unhover_menu_ancestors(GtkWidget* popover) {
  auto* motion = gtk_event_controller_motion_new();
  g_signal_connect(
      motion, "enter", G_CALLBACK(+[](GtkEventController* controller, double /*x*/, double /*y*/) {
        for (auto* widget = gtk_widget_get_parent(gtk_event_controller_get_widget(controller));
             widget != nullptr; widget = gtk_widget_get_parent(widget)) {
          gtk_widget_unset_state_flags(widget, GTK_STATE_FLAG_PRELIGHT);
        }
      }),
      nullptr);
  gtk_widget_add_controller(popover, motion);
}

}  // namespace waybar::util
