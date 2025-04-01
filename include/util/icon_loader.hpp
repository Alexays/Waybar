#pragma once

#include <gdkmm/general.h>
#include <gio/gdesktopappinfo.h>
#include <giomm/desktopappinfo.h>
#include <glibmm/fileutils.h>
#include <gtkmm/image.h>
#include <spdlog/spdlog.h>

#include <string>
#include <vector>

#include "util/gtk_icon.hpp"

class IconLoader {
 private:
  std::vector<Glib::RefPtr<Gtk::IconTheme>> custom_icon_themes_;
  Glib::RefPtr<Gtk::IconTheme> default_icon_theme_ = Gtk::IconTheme::get_default();
  static std::vector<std::string> search_prefix();
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_by_name(const std::string &app_id);
  static Glib::RefPtr<Gio::DesktopAppInfo> get_desktop_app_info(const std::string &app_id);
  static Glib::RefPtr<Gdk::Pixbuf> load_icon_from_file(std::string const &icon_path, int size);
  static std::string get_icon_name_from_icon_theme(const Glib::RefPtr<Gtk::IconTheme> &icon_theme,
                                                   const std::string &app_id);
  static bool image_load_icon(Gtk::Image &image, const Glib::RefPtr<Gtk::IconTheme> &icon_theme,
                              Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size);

 public:
  void add_custom_icon_theme(const std::string &theme_name);
  bool image_load_icon(Gtk::Image &image, Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                       int size) const;
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_from_app_id_list(
      const std::string &app_id_list);
};
