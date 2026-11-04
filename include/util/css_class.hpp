#pragma once

#include <glib.h>
#include <gtkmm/stylecontext.h>

#include <string>

namespace waybar::util {

// Adds a class named by another program, and returns whether it did. GTK interns every class name
// as a GQuark, which GLib never frees, so only a name that a loaded stylesheet already interned
// (one that a selector can match) is added; a skipped name is tried again on the next update.
inline bool try_add_class(const Glib::RefPtr<Gtk::StyleContext>& context, const std::string& name) {
  if (g_quark_try_string(name.c_str()) == 0) {
    return false;
  }
  gtk_style_context_add_class(context->gobj(), name.c_str());
  return true;
}

}  // namespace waybar::util
