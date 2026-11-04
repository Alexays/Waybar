#pragma once

#include <glib.h>
#include <gtkmm/stylecontext.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

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

// Whether name is a class Waybar sets on the bar window for its own state (hidden, position,
// mode-*, ...), which a window's app_id must neither fake nor take away.
inline bool is_bar_state_class(std::string_view name) {
  static constexpr std::array<std::string_view, 13> names = {
      "hidden", "top",     "bottom", "left",     "right",      "empty",     "solo",
      "tabbed", "stacked", "tiled",  "floating", "swallowing", "fullscreen"};
  static constexpr std::array<std::string_view, 3> prefixes = {"mode-", "battery-", "toplevel-"};
  return std::ranges::find(names, name) != names.end() ||
         std::ranges::any_of(prefixes, [name](auto prefix) { return name.starts_with(prefix); });
}

// Adds a window's app_id for window#waybar.<app_id>, unless it names the bar's own state or a class
// the widget already has (output or bar name). Returns whether it did: only then may it be removed.
inline bool add_app_id_class(const Glib::RefPtr<Gtk::StyleContext>& context,
                             const std::string& app_id) {
  if (app_id.empty() || is_bar_state_class(app_id) ||
      gtk_style_context_has_class(context->gobj(), app_id.c_str())) {
    return false;
  }
  return try_add_class(context, app_id);
}

}  // namespace waybar::util
