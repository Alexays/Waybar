#include "AAppIconLabel.hpp"

#include <gdkmm/pixbuf.h>
#include <glibmm/exception.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <optional>

#include "util/gtk/gtk_icon.hpp"

namespace waybar {

AAppIconLabel::AAppIconLabel(const Json::Value& config, const std::string& name,
                             const std::string& id, const std::string& format, uint16_t interval,
                             bool ellipsize, bool enable_click, bool enable_scroll)
    : AIconLabel(config, name, id, format, interval, ellipsize, enable_click, enable_scroll) {
  // Icon size
  if (config["icon-size"].isUInt()) {
    app_icon_size_ = config["icon-size"].asUInt();
  }
  image_.set_pixel_size(app_icon_size_);
}

std::string toLowerCase(const std::string& input) {
  std::string result = input;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

std::optional<std::string> getFileBySuffix(const std::string& dir, const std::string& suffix,
                                           bool check_lower_case) {
  try {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
      if (entry.is_regular_file()) {
        std::string filename = entry.path().filename().string();
        if (filename.size() < suffix.size()) {
          continue;
        }
        if ((filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) ||
            (check_lower_case && filename.compare(filename.size() - suffix.size(), suffix.size(),
                                                  toLowerCase(suffix)) == 0)) {
          return entry.path().string();
        }
      }
    }
  } catch (const std::filesystem::filesystem_error&) {
  }

  return {};
}

std::optional<std::string> getFileBySuffix(const std::string& dir, const std::string& suffix) {
  return getFileBySuffix(dir, suffix, false);
}

std::optional<std::string> getDesktopFilePath(const std::string& app_identifier,
                                              const std::string& alternative_app_identifier) {
  if (app_identifier.empty()) {
    return {};
  }

  auto data_dirs = Glib::get_system_data_dirs();
  data_dirs.insert(data_dirs.begin(), Glib::get_user_data_dir());
  for (const auto& data_dir : data_dirs) {
    const auto data_app_dir = data_dir + "/applications/";
    auto desktop_file_suffix = app_identifier + ".desktop";
    // searching for file by suffix catches cases like terminal emulator "foot" where class is
    // "footclient" and desktop file is named "org.codeberg.dnkl.footclient.desktop"
    auto desktop_file_path = getFileBySuffix(data_app_dir, desktop_file_suffix, true);
    // "true" argument allows checking for lowercase - this catches cases where class name is
    // "LibreWolf" and desktop file is named "librewolf.desktop"
    if (desktop_file_path.has_value()) {
      return desktop_file_path;
    }
    if (!alternative_app_identifier.empty()) {
      desktop_file_suffix = alternative_app_identifier + ".desktop";
      desktop_file_path = getFileBySuffix(data_app_dir, desktop_file_suffix, true);
      if (desktop_file_path.has_value()) {
        return desktop_file_path;
      }
    }
  }
  return {};
}

std::optional<Glib::ustring> getIconName(const std::string& app_identifier,
                                         const std::string& alternative_app_identifier) {
  const auto desktop_file_path = getDesktopFilePath(app_identifier, alternative_app_identifier);
  if (!desktop_file_path.has_value()) {
    if (util::DefaultGtkIconThemeWrapper::has_icon(app_identifier)) {
      return app_identifier;
    }

    auto app_identifier_desktop = app_identifier + "-desktop";
    if (util::DefaultGtkIconThemeWrapper::has_icon(app_identifier_desktop)) {
      return app_identifier_desktop;
    }

    auto first_space = app_identifier.find_first_of(' ');
    if (first_space != std::string::npos) {
      auto first_word = toLowerCase(app_identifier.substr(0, first_space));
      if (util::DefaultGtkIconThemeWrapper::has_icon(first_word)) {
        return first_word;
      }
    }

    const auto first_dash = app_identifier.find_first_of('-');
    if (first_dash != std::string::npos) {
      auto first_word = toLowerCase(app_identifier.substr(0, first_dash));
      if (util::DefaultGtkIconThemeWrapper::has_icon(first_word)) {
        return first_word;
      }
    }

    return {};
  }

  try {
    auto app_info = Gio::DesktopAppInfo::create_from_filename(desktop_file_path.value());
    if (!app_info) {
      return {};
    }

    auto icon = app_info->get_icon();
    if (!icon) {
      return {};
    }

    if (auto themed = std::dynamic_pointer_cast<Gio::ThemedIcon>(icon)) {
      const auto& names = themed->get_names();
      if (!names.empty()) {
        return names[0];
      }
    }

    if (auto file_icon = std::dynamic_pointer_cast<Gio::FileIcon>(icon)) {
      auto file = file_icon->get_file();
      if (file) {
        return file->get_path();
      }
    }

    return icon->to_string();
  } catch (const Glib::Error& error) {
    spdlog::warn("Error while loading desktop file {}: {}", desktop_file_path.value(),
                 std::string(error.what()));
  }

  return {};
}

void AAppIconLabel::updateAppIconName(const std::string& app_identifier,
                                      const std::string& alternative_app_identifier) {
  if (!iconEnabled()) {
    return;
  }

  const auto icon_name = getIconName(app_identifier, alternative_app_identifier);
  if (icon_name.has_value()) {
    app_icon_name_ = icon_name.value();
  } else {
    app_icon_name_ = "";
  }
  update_app_icon_ = true;
}

void AAppIconLabel::updateAppIcon() {
  if (update_app_icon_ || (!iconEnabled() && image_.get_visible())) {
    update_app_icon_ = false;
    if (app_icon_name_.empty()) {
      image_.set_visible(false);
    } else if (app_icon_name_.front() == '/') {
      try {
        int scaled_icon_size = app_icon_size_ * image_.get_scale_factor();
        auto pixbuf =
            Gdk::Pixbuf::create_from_file(app_icon_name_, scaled_icon_size, scaled_icon_size);
        auto texture = Gdk::Texture::create_for_pixbuf(pixbuf);

        image_.property_paintable().set_value(texture);
        image_.set_visible(true);
      } catch (const Glib::Exception& e) {
        spdlog::warn("Failed to load app icon {}: {}", app_icon_name_, std::string(e.what()));
        image_.set_visible(false);
      }
    } else {
      image_.set_from_icon_name(app_icon_name_);
      image_.set_visible(true);
    }
  }
}

auto AAppIconLabel::doUpdate() -> void {
  updateAppIcon();
  AIconLabel::doUpdate();
}

}  // namespace waybar
