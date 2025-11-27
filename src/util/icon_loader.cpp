#include "util/icon_loader.hpp"

#include "util/string.hpp"

std::vector<std::string> IconLoader::search_prefix() {
  std::vector<std::string> prefixes = {""};

  std::string home_dir = std::getenv("HOME");
  prefixes.push_back(home_dir + "/.local/share/");

  auto xdg_data_dirs = std::getenv("XDG_DATA_DIRS");
  if (!xdg_data_dirs) {
    prefixes.emplace_back("/usr/share/");
    prefixes.emplace_back("/usr/local/share/");
  } else {
    std::string xdg_data_dirs_str(xdg_data_dirs);
    size_t start = 0;
    size_t end = 0;

    do {
      end = xdg_data_dirs_str.find(':', start);
      auto p = xdg_data_dirs_str.substr(start, end - start);
      prefixes.push_back(trim(p) + "/");

      start = end == std::string::npos ? end : end + 1;
    } while (end != std::string::npos);
  }

  for (auto &p : prefixes) spdlog::debug("Using 'desktop' search path prefix: {}", p);

  return prefixes;
}

Glib::RefPtr<Gio::DesktopAppInfo> IconLoader::get_app_info_by_name(const std::string &app_id) {
  static std::vector<std::string> prefixes = search_prefix();

  std::vector<std::string> app_folders = {"", "applications/", "applications/kde/",
                                          "applications/org.kde."};

  std::vector<std::string> suffixes = {"", ".desktop"};

  for (auto const &prefix : prefixes) {
    for (auto const &folder : app_folders) {
      for (auto const &suffix : suffixes) {
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

Glib::RefPtr<Gio::DesktopAppInfo> IconLoader::get_desktop_app_info(const std::string &app_id) {
  auto app_info = get_app_info_by_name(app_id);
  if (app_info) {
    return app_info;
  }

  std::string desktop_file = "";

  gchar ***desktop_list = g_desktop_app_info_search(app_id.c_str());
  if (desktop_list != nullptr && desktop_list[0] != nullptr) {
    for (size_t i = 0; desktop_list[0][i]; i++) {
      if (desktop_file == "") {
        desktop_file = desktop_list[0][i];
      } else {
        auto tmp_info = Gio::DesktopAppInfo::create(desktop_list[0][i]);
        if (!tmp_info)
          // see https://github.com/Alexays/Waybar/issues/1446
          continue;

        auto startup_class = tmp_info->get_startup_wm_class();
        if (startup_class == app_id) {
          desktop_file = desktop_list[0][i];
          break;
        }
      }
    }
    g_strfreev(desktop_list[0]);
  }
  g_free(desktop_list);

  return get_app_info_by_name(desktop_file);
}

Glib::RefPtr<Gdk::Pixbuf> IconLoader::load_icon_from_file(std::string const &icon_path, int size) {
  try {
    auto pb = Gdk::Pixbuf::create_from_file(icon_path, size, size);
    return pb;
  } catch (...) {
    return {};
  }
}

std::string IconLoader::get_icon_name_from_icon_theme(
    const Glib::RefPtr<Gtk::IconTheme> &icon_theme, const std::string &app_id) {
  if (icon_theme->lookup_icon(app_id, 24)) return app_id;

  return "";
}

bool IconLoader::image_load_icon(Gtk::Image &image, const Glib::RefPtr<Gtk::IconTheme> &icon_theme,
                                 Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size) {
  std::string ret_icon_name = "unknown";
  if (app_info) {
    std::string icon_name =
        get_icon_name_from_icon_theme(icon_theme, app_info->get_startup_wm_class());
    if (!icon_name.empty()) {
      ret_icon_name = icon_name;
    } else {
      if (app_info->get_icon()) {
        ret_icon_name = app_info->get_icon()->to_string();
      }
    }
  }

  Glib::RefPtr<Gdk::Pixbuf> pixbuf;
  auto scaled_icon_size = size * image.get_scale_factor();

  try {
    pixbuf = icon_theme->load_icon(ret_icon_name, scaled_icon_size, Gtk::ICON_LOOKUP_FORCE_SIZE);
  } catch (...) {
    if (Glib::file_test(ret_icon_name, Glib::FILE_TEST_EXISTS)) {
      pixbuf = load_icon_from_file(ret_icon_name, scaled_icon_size);
    } else {
      try {
        pixbuf = DefaultGtkIconThemeWrapper::load_icon(
            "image-missing", scaled_icon_size, Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE);
      } catch (...) {
        pixbuf = {};
      }
    }
  }

  if (pixbuf) {
    if (pixbuf->get_width() != scaled_icon_size) {
      int width = scaled_icon_size * pixbuf->get_width() / pixbuf->get_height();
      pixbuf = pixbuf->scale_simple(width, scaled_icon_size, Gdk::InterpType::INTERP_BILINEAR);
    }
    auto surface = Gdk::Cairo::create_surface_from_pixbuf(pixbuf, image.get_scale_factor(),
                                                          image.get_window());
    image.set(surface);
    return true;
  }

  return false;
}

void IconLoader::add_custom_icon_theme(const std::string &theme_name) {
  auto icon_theme = Gtk::IconTheme::create();
  icon_theme->set_custom_theme(theme_name);
  custom_icon_themes_.push_back(icon_theme);
  spdlog::debug("Use custom icon theme: {}", theme_name);
}

bool IconLoader::image_load_icon(Gtk::Image &image, Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                                 int size) const {
  for (auto &icon_theme : custom_icon_themes_) {
    if (image_load_icon(image, icon_theme, app_info, size)) {
      return true;
    }
  }
  return image_load_icon(image, default_icon_theme_, app_info, size);
}

Glib::RefPtr<Gio::DesktopAppInfo> IconLoader::get_app_info_from_app_id_list(
    const std::string &app_id_list) {
  std::string app_id;
  std::istringstream stream(app_id_list);
  Glib::RefPtr<Gio::DesktopAppInfo> app_info_;

  /* Wayfire sends a list of app-id's in space separated format, other compositors
   * send a single app-id, but in any case this works fine */
  while (stream >> app_id) {
    app_info_ = get_desktop_app_info(app_id);
    if (app_info_) {
      return app_info_;
    }

    auto lower_app_id = app_id;
    std::ranges::transform(lower_app_id, lower_app_id.begin(),
                           [](char c) { return std::tolower(c); });
    app_info_ = get_desktop_app_info(lower_app_id);
    if (app_info_) {
      return app_info_;
    }

    size_t start = 0, end = app_id.size();
    start = app_id.rfind(".", end);
    std::string app_name = app_id.substr(start + 1, app_id.size());
    app_info_ = get_desktop_app_info(app_name);
    if (app_info_) {
      return app_info_;
    }

    start = app_id.find("-");
    app_name = app_id.substr(0, start);
    app_info_ = get_desktop_app_info(app_name);
  }
  return app_info_;
}
