#pragma once
#include <gdkmm/pixbuf.h>
#include <gtkmm/icontheme.h>

#include <mutex>
#include <string>

class DefaultGtkIconThemeWrapper {
 private:
  static std::mutex default_theme_mutex;

 public:
  static Glib::RefPtr<Gtk::IconTheme> theme();
  static bool has_icon(const std::string&);
  // The themed icon `name` rendered at `size` pixels, null when the theme has none.
  static Glib::RefPtr<Gdk::Pixbuf> load_icon(const std::string& name, int size);
};
