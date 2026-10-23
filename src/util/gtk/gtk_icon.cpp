#include "util/gtk/gtk_icon.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace waybar::util {

std::mutex DefaultGtkIconThemeWrapper::default_theme_mutex;

Glib::RefPtr<Gtk::IconTheme> DefaultGtkIconThemeWrapper::get_theme() {
  const std::lock_guard<std::mutex> lock(default_theme_mutex);
  return Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
}

bool DefaultGtkIconThemeWrapper::has_icon(const std::string& value) {
  return get_theme()->has_icon(value);
}

Glib::RefPtr<Gdk::Paintable> DefaultGtkIconThemeWrapper::load_icon(const char* name, int size,
                                                                   Gtk::IconLookupFlags flags) {
  // lookup_icon in GTK4 returns an IconPaintable (which is a Paintable).
  // Symbolic colors are applied automatically when the widget renders it.
  return get_theme()->lookup_icon(name, size, 1, Gtk::TextDirection::NONE, flags);
}

namespace gtk {

Glib::RefPtr<Gio::DesktopAppInfo> HIcon::get_app_info_by_name(const std::string& app_id) {
  static std::vector<std::string> prefixes = waybar::util::search_prefix();

  std::vector<std::string> app_folders = {"", "applications/", "applications/kde/",
                                          "applications/org.kde."};

  std::vector<std::string> suffixes = {"", ".desktop"};

  for (auto const& prefix : prefixes) {
    for (auto const& folder : app_folders) {
      for (auto const& suffix : suffixes) {
        auto app_info_ =
            Gio::DesktopAppInfo::create_from_filename(prefix + folder + app_id + suffix);
        if (!app_info_) {
          continue;
        }

        return app_info_;
      }
    }
  }

  return {};
}

Glib::RefPtr<Gio::DesktopAppInfo> HIcon::get_app_info(const std::string& app_id) {
  if (auto app_info = get_app_info_by_name(app_id)) {
    return app_info;
  }

  std::string desktop_file;
  const auto all_apps = Gio::AppInfo::get_all();

  for (const auto& app_info : all_apps) {
    const auto id = app_info->get_id();
    if (id.empty()) {
      continue;
    }

    if (desktop_file.empty()) {
      desktop_file = id;
    }

    auto tmp_info = Gio::DesktopAppInfo::create(id);
    if (!tmp_info) {
      continue;
    }

    if (tmp_info->get_startup_wm_class() == app_id) {
      desktop_file = id;
      break;
    }
  }

  return get_app_info_by_name(desktop_file);
}

Glib::RefPtr<Gio::DesktopAppInfo> HIcon::get_app_info_by_list(const std::string& app_id) {
  Glib::RefPtr<Gio::DesktopAppInfo> app_info_{get_app_info(app_id)};
  if (app_info_) {
    return app_info_;
  }

  auto lower_app_id = app_id;
  std::ranges::transform(lower_app_id, lower_app_id.begin(),
                         [](char c) { return std::tolower(c); });
  app_info_ = get_app_info(lower_app_id);
  if (app_info_) {
    return app_info_;
  }

  size_t start = 0, end = app_id.size();
  start = app_id.rfind(".", end);
  std::string app_name = app_id.substr(start + 1, app_id.size());
  app_info_ = get_app_info(app_name);
  if (app_info_) {
    return app_info_;
  }

  start = app_id.find("-");
  app_name = app_id.substr(0, start);
  app_info_ = get_app_info(app_name);

  return app_info_;
}

bool HIcon::image_load_icon(Gtk::Image& image, const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                            Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size) {
  const auto icon{app_info->get_icon()};
  const auto paintable{icon_theme->lookup_icon(icon, size, image.get_scale_factor(),
                                               image.get_direction(), Gtk::IconLookupFlags::NONE)};

  if (paintable) {
    image.property_paintable().set_value(paintable);
    return true;
  }

  return false;
}

bool HIcon::image_load_icon(Gtk::Image& image, Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                            int size) {
  for (auto& icon_theme : custom_icon_themes_) {
    if (image_load_icon(image, icon_theme, app_info, size)) {
      return true;
    }
  }

  return image_load_icon(image, DefaultGtkIconThemeWrapper::get_theme(), app_info, size);
}

void HIcon::add_custom_icon_theme(const std::string& theme_name) {
  auto icon_theme = Gtk::IconTheme::create();
  icon_theme->set_theme_name(theme_name);
  custom_icon_themes_.push_back(icon_theme);
  spdlog::debug("Use custom icon theme: {}", theme_name);
}

}  // namespace gtk

}  // namespace waybar::util
