#include "AAppIconLabel.hpp"

#include <gdkmm/pixbuf.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <optional>

#include "util/gtk_icon.hpp"

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

std::optional<std::string> getDesktopFilePath(const std::string& app_identifier,
                                              const std::string& alternative_app_identifier) {
  const auto data_dirs = Glib::get_system_data_dirs();
  for (const auto& data_dir : data_dirs) {
    const auto data_app_dir = data_dir + "applications/";
    auto desktop_file_path = data_app_dir + app_identifier + ".desktop";
    if (std::filesystem::exists(desktop_file_path)) {
      return desktop_file_path;
    }
    if (!alternative_app_identifier.empty()) {
      desktop_file_path = data_app_dir + alternative_app_identifier + ".desktop";
      if (std::filesystem::exists(desktop_file_path)) {
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
    // Try some heuristics to find a matching icon

    if (DefaultGtkIconThemeWrapper::has_icon(app_identifier)) {
      return app_identifier;
    }

    const auto app_identifier_desktop = app_identifier + "-desktop";
    if (DefaultGtkIconThemeWrapper::has_icon(app_identifier_desktop)) {
      return app_identifier_desktop;
    }

    const auto to_lower = [](const std::string& str) {
      auto str_cpy = str;
      std::transform(str_cpy.begin(), str_cpy.end(), str_cpy.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      return str;
    };

    const auto first_space = app_identifier.find_first_of(' ');
    if (first_space != std::string::npos) {
      const auto first_word = to_lower(app_identifier.substr(0, first_space));
      if (DefaultGtkIconThemeWrapper::has_icon(first_word)) {
        return first_word;
      }
    }

    const auto first_dash = app_identifier.find_first_of('-');
    if (first_dash != std::string::npos) {
      const auto first_word = to_lower(app_identifier.substr(0, first_dash));
      if (DefaultGtkIconThemeWrapper::has_icon(first_word)) {
        return first_word;
      }
    }

    return {};
  }

  try {
    Glib::KeyFile desktop_file;
    desktop_file.load_from_file(desktop_file_path.value());
    return desktop_file.get_string("Desktop Entry", "Icon");
  } catch (Glib::FileError& error) {
    spdlog::warn("Error while loading desktop file {}: {}", desktop_file_path.value(),
                 error.what().c_str());
  } catch (Glib::KeyFileError& error) {
    spdlog::warn("Error while loading desktop file {}: {}", desktop_file_path.value(),
                 error.what().c_str());
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
  if (update_app_icon_) {
    update_app_icon_ = false;
    if (app_icon_name_.empty()) {
      image_.set_visible(false);
    } else {
      image_.set_from_icon_name(app_icon_name_, Gtk::ICON_SIZE_INVALID);
      image_.set_visible(true);
    }
  }
}

auto AAppIconLabel::update() -> void {
  updateAppIcon();
  AIconLabel::update();
}

}  // namespace waybar
