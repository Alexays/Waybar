#pragma once

#include <gdkmm/general.h>
#include <gio/gdesktopappinfo.h>
#include <giomm/desktopappinfo.h>
#include <glibmm/fileutils.h>
#include <gtkmm/image.h>
#include <spdlog/spdlog.h>

#include <string>
#include <utility>
#include <vector>

#include "util/gtk_icon.hpp"

class IconLoader {
 private:
  // A configured icon theme together with the name it was configured under. The
  // name is what lets us tell an icon the theme itself ships from one it merely
  // reaches through its Inherits= chain; gtkmm offers no way to read the name
  // back off a Gtk::IconTheme.
  using NamedIconTheme = std::pair<std::string, Glib::RefPtr<Gtk::IconTheme>>;

  std::vector<NamedIconTheme> custom_icon_themes_;
  Glib::RefPtr<Gtk::IconTheme> default_icon_theme_ = Gtk::IconTheme::get_default();
  static std::vector<std::string> search_prefix();
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_by_name(const std::string& app_id);
  static Glib::RefPtr<Gio::DesktopAppInfo> get_desktop_app_info(const std::string& app_id,
                                                                guint& search_words_left);
  static Glib::RefPtr<Gdk::Pixbuf> load_icon_from_file(std::string const& icon_path, int size);
  static std::string get_icon_name_from_icon_theme(const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                                                   const std::string& theme_name,
                                                   const std::string& app_id);
  static bool image_load_icon(Gtk::Image& image, const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                              const std::string& theme_name,
                              Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size);

 public:
  void add_custom_icon_theme(const std::string& theme_name);
  bool image_load_icon(Gtk::Image& image, Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                       int size) const;
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_from_app_id_list(
      const std::string& app_id_list);
  // True only when `icon_name` resolves to a file inside `theme_name`'s own
  // directory. Gtk::IconTheme::lookup_icon() follows Inherits=, so a plain
  // lookup cannot answer "does *this* theme provide it?". Public so the
  // inheritance behavior it guards against can be covered by a test.
  static bool icon_is_native_to_theme(const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                                      const std::string& theme_name, const std::string& icon_name,
                                      int size);
};
