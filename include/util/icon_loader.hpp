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
 public:
  /* LOADED is a real icon, MISSING the theme's image-missing fallback, FAILED means no surface
   * was set at all */
  enum class LoadResult { LOADED, MISSING, FAILED };

 private:
  std::vector<Glib::RefPtr<Gtk::IconTheme>> custom_icon_themes_;
  Glib::RefPtr<Gtk::IconTheme> default_icon_theme_ = Gtk::IconTheme::get_default();
  static std::vector<std::string> search_prefix();
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_by_name(const std::string& app_id);
  static Glib::RefPtr<Gio::DesktopAppInfo> get_desktop_app_info(const std::string& app_id,
                                                                guint& search_words_left);
  static Glib::RefPtr<Gdk::Pixbuf> load_icon_from_file(std::string const& icon_path, int size);
  static std::string get_icon_name_from_icon_theme(const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                                                   const std::string& app_id);
  static std::string icon_name_for(const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                                   const Glib::RefPtr<Gio::DesktopAppInfo>& app_info);
  static LoadResult image_load_icon(Gtk::Image& image,
                                    const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                                    const std::string& icon_name, int size);
  static bool set_missing_icon(Gtk::Image& image, int size);

 public:
  void add_custom_icon_theme(const std::string& theme_name);
  LoadResult image_load_icon(Gtk::Image& image, Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                             int size) const;
  LoadResult image_load_icon(Gtk::Image& image, const std::string& app_id, int size) const;
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_from_app_id_list(
      const std::string& app_id_list);
};
