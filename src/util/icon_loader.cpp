#include "util/icon_loader.hpp"

#include <algorithm>

#include "util/string.hpp"

std::vector<std::string> IconLoader::search_prefix() {
  std::vector<std::string> prefixes;

  const char* home_env = std::getenv("HOME");
  std::string home_dir = home_env ? home_env : "";
  if (!home_dir.empty()) {
    prefixes.push_back(home_dir + "/.local/share/");
  }

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

  for (auto& p : prefixes) spdlog::debug("Using 'desktop' search path prefix: {}", p);

  return prefixes;
}

Glib::RefPtr<Gio::DesktopAppInfo> IconLoader::get_app_info_by_name(const std::string& app_id) {
  static std::vector<std::string> prefixes = search_prefix();

  // App ids come from other clients: they name a desktop entry and are never a path
  if (app_id.empty() || app_id.find('/') != std::string::npos) {
    return {};
  }

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

Glib::RefPtr<Gio::DesktopAppInfo> IconLoader::get_desktop_app_info(const std::string& app_id,
                                                                   guint& search_words_left) {
  auto app_info = get_app_info_by_name(app_id);
  if (app_info) {
    return app_info;
  }

  // App ids come from other clients and g_desktop_app_info_search() crashes on invalid UTF-8
  if (!g_utf8_validate(app_id.c_str(), app_id.size(), nullptr)) {
    return {};
  }

  // The search walks the index of every installed application once per word of app_id
  gchar** search_words = g_str_tokenize_and_fold(app_id.c_str(), nullptr, nullptr);
  const guint n_search_words = g_strv_length(search_words);
  g_strfreev(search_words);
  if (n_search_words == 0 || n_search_words > search_words_left) {
    return {};
  }
  search_words_left -= n_search_words;

  std::string desktop_file = "";

  gchar*** desktop_list = g_desktop_app_info_search(app_id.c_str());
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
  }
  if (desktop_list != nullptr) {
    // One strv per match score, all owned by the caller
    for (size_t group = 0; desktop_list[group]; group++) {
      g_strfreev(desktop_list[group]);
    }
    g_free(desktop_list);
  }

  if (desktop_file.empty()) {
    return {};
  }
  return get_app_info_by_name(desktop_file);
}

bool IconLoader::image_load_icon(Gtk::Image& image, const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                                 Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size) {
  Glib::RefPtr<Gtk::IconPaintable> paintable;
  const int scale = image.get_scale_factor();
  if (app_info) {
    const std::string wm_class = app_info->get_startup_wm_class();
    if (!wm_class.empty() && icon_theme->has_icon(wm_class)) {
      paintable = icon_theme->lookup_icon(wm_class, size, scale);
    } else if (auto icon = app_info->get_icon()) {
      // A themed name or a file; GTK falls back to image-missing
      paintable = icon_theme->lookup_icon(icon, size, scale);
    }
  }
  if (!paintable) {
    paintable = icon_theme->lookup_icon("unknown", size, scale);
  }
  image.set(paintable);
  image.set_pixel_size(size);
  return true;
}

void IconLoader::add_custom_icon_theme(const std::string& theme_name) {
  auto icon_theme = Gtk::IconTheme::create();
  icon_theme->set_theme_name(theme_name);
  custom_icon_themes_.push_back(icon_theme);
  spdlog::debug("Use custom icon theme: {}", theme_name);
}

bool IconLoader::image_load_icon(Gtk::Image& image, Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                                 int size) const {
  for (auto& icon_theme : custom_icon_themes_) {
    if (image_load_icon(image, icon_theme, app_info, size)) {
      return true;
    }
  }
  return image_load_icon(image, default_icon_theme_, app_info, size);
}

Glib::RefPtr<Gio::DesktopAppInfo> IconLoader::get_app_info_from_app_id_list(
    const std::string& app_id_list) {
  std::string app_id;
  std::istringstream stream(app_id_list);
  // Bounds the cost of the desktop app searches of the whole list, whatever its shape
  guint search_words_left = 32;

  /* Wayfire sends a list of app-id's in space separated format, other compositors
   * send a single app-id. The list comes from other clients and a miss costs hundreds of file
   * probes on the main thread, so only the first entries are tried */
  for (int i = 0; i < 8 && stream >> app_id; i++) {
    auto app_info = get_desktop_app_info(app_id, search_words_left);
    if (app_info) {
      return app_info;
    }

    auto lower_app_id = app_id;
    std::ranges::transform(lower_app_id, lower_app_id.begin(), [](char c) {
      return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    if (lower_app_id != app_id) {
      app_info = get_desktop_app_info(lower_app_id, search_words_left);
      if (app_info) {
        return app_info;
      }
    }

    // npos + 1 wraps to 0: no '.' keeps the whole id
    auto app_name = app_id.substr(app_id.rfind('.') + 1);
    if (app_name != app_id) {
      app_info = get_desktop_app_info(app_name, search_words_left);
      if (app_info) {
        return app_info;
      }
    }

    app_name = app_id.substr(0, app_id.find('-'));
    if (app_name != app_id) {
      app_info = get_desktop_app_info(app_name, search_words_left);
      if (app_info) {
        return app_info;
      }
    }
  }
  return {};
}
