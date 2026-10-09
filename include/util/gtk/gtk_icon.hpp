#pragma once
#include <gdkmm/paintable.h>
#include <giomm/desktopappinfo.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>

#include <mutex>
#include <string>

#include "util/common.hpp"

namespace waybar::util {

class DefaultGtkIconThemeWrapper {
 private:
  static std::mutex default_theme_mutex;

 public:
  static bool has_icon(const std::string&);
  static Glib::RefPtr<Gdk::Paintable> load_icon(
      const char* name, int size, Gtk::IconLookupFlags flags = Gtk::IconLookupFlags::FORCE_REGULAR);
  static Glib::RefPtr<Gtk::IconTheme> get_theme();
};

namespace gtk {
// Helpers
class HIcon {
 public:
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_by_name(const std::string& app_id);
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_by_list(const std::string& app_id);
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info(const std::string& app_id);

  static bool image_load_icon(Gtk::Image& image, std::string app_id, int size);
  static bool image_load_icon(Gtk::Image& image, Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                              int size);
  static void add_custom_icon_theme(const std::string& theme_name);

 private:
  static bool image_load_icon(Gtk::Image& image, const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                              Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size);
  static inline std::vector<Glib::RefPtr<Gtk::IconTheme>> custom_icon_themes_;
};

}  // namespace gtk

}  // namespace waybar::util
