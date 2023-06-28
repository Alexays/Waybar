#pragma once
#include <string>
#include <mutex>
#include <gtkmm/icontheme.h>

class DefaultGtkIconThemeWrapper {
  private:
    static std::mutex default_theme_mutex;
  public:
    static bool has_icon(const std::string&);
    static Glib::RefPtr<Gdk::Pixbuf> load_icon(const char*, int, Gtk::IconLookupFlags);
};
