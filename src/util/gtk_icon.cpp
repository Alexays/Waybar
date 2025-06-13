#include "util/gtk_icon.hpp"

/* We need a global mutex for accessing the object returned by Gtk::IconTheme::get_default()
 * because it always returns the same object across different threads, and concurrent
 * access can cause data corruption and lead to invalid memory access and crashes.
 * Even concurrent calls that seem read only such as has_icon can cause issues because
 * the GTK lib may update the internal icon cache on this calls.
 */

std::mutex DefaultGtkIconThemeWrapper::default_theme_mutex;

bool DefaultGtkIconThemeWrapper::has_icon(const std::string& value) {
  const std::lock_guard<std::mutex> lock(default_theme_mutex);

  return Gtk::IconTheme::get_default()->has_icon(value);
}

Glib::RefPtr<Gdk::Pixbuf> DefaultGtkIconThemeWrapper::load_icon(
    const char* name, int tmp_size, Gtk::IconLookupFlags flags,
    Glib::RefPtr<Gtk::StyleContext> style) {
  const std::lock_guard<std::mutex> lock(default_theme_mutex);

  auto default_theme = Gtk::IconTheme::get_default();
  default_theme->rescan_if_needed();

  auto icon_info = default_theme->lookup_icon(name, tmp_size, flags);

  if (style.get() == nullptr) {
    return icon_info.load_icon();
  }

  bool is_sym = false;
  return icon_info.load_symbolic(style, is_sym);
}
