#pragma once

#include <giomm/desktopappinfo.h>
#include <glibmm/refptr.h>

#include <optional>
#include <string>
#include <string_view>

namespace waybar::util::steam {

std::optional<std::string> extract_app_id(std::string_view app_id);
bool is_default_app_id(std::string_view app_id);
bool is_fallback_app_id(std::string_view app_id);
bool has_app_id(const std::string& app_id_list);
bool should_skip_title_fallback(const std::string& app_id_list);
std::optional<std::string> extract_fallback_from_list(const std::string& app_id_list);
Glib::RefPtr<Gio::DesktopAppInfo> find_desktop_app_info(const std::string& steam_app_id);
std::string find_library_icon(const std::string& steam_app_id);

}  // namespace waybar::util::steam
