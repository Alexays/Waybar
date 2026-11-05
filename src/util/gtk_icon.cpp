#include "util/gtk_icon.hpp"

#include <gdkmm/display.h>

/* The icon theme of the default display is shared by every caller, some of which run outside of
 * the main thread, and GTK updates its internal icon cache even on read only calls such as
 * has_icon: serialize every access.
 */

std::mutex DefaultGtkIconThemeWrapper::default_theme_mutex;

Glib::RefPtr<Gtk::IconTheme> DefaultGtkIconThemeWrapper::theme() {
  return Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
}

bool DefaultGtkIconThemeWrapper::has_icon(const std::string& value) {
  const std::lock_guard<std::mutex> lock(default_theme_mutex);
  return theme()->has_icon(value);
}

Glib::RefPtr<Gdk::Pixbuf> DefaultGtkIconThemeWrapper::load_icon(const std::string& name, int size) {
  Glib::RefPtr<Gio::File> file;
  {
    const std::lock_guard<std::mutex> lock(default_theme_mutex);
    auto icon_theme = theme();
    if (!icon_theme->has_icon(name)) {
      return {};
    }
    auto paintable = icon_theme->lookup_icon(name, size, 1);
    file = paintable ? paintable->get_file() : nullptr;
  }
  if (!file) {
    return {};
  }
  try {
    if (file->has_uri_scheme("resource")) {
      constexpr std::string_view prefix = "resource://";
      return Gdk::Pixbuf::create_from_resource(file->get_uri().substr(prefix.size()), size, size,
                                               true);
    }
    return Gdk::Pixbuf::create_from_file(file->get_path(), size, size, true);
  } catch (const Glib::Error&) {
    return {};
  }
}
