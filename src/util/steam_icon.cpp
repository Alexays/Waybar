#include "util/steam_icon.hpp"

#include <gio/gdesktopappinfo.h>
#include <glib.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <ranges>
#include <sstream>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include "util/string.hpp"

namespace waybar::util::steam {

namespace {

namespace fs = std::filesystem;

bool ascii_iequals(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::ranges::equal(a, b, [](unsigned char ca, unsigned char cb) {
           auto lower = [](unsigned char c) -> unsigned char {
             return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c - 'A' + 'a') : c;
           };
           return lower(ca) == lower(cb);
         });
}

bool is_nonzero_steam_app_id(std::string_view steam_app_id) {
  return !steam_app_id.empty() && steam_app_id.size() <= 20 &&
         std::ranges::all_of(steam_app_id, [](unsigned char c) { return c >= '0' && c <= '9'; }) &&
         steam_app_id.find_first_not_of('0') != std::string_view::npos;
}

std::string_view safe_sv(const char* s) { return s ? std::string_view(s) : std::string_view{}; }

bool has_token_boundary_before(std::string_view s, size_t pos) {
  if (pos == 0) {
    return true;
  }
  const unsigned char c = static_cast<unsigned char>(s[pos - 1]);
  return std::isspace(c) != 0 || c == '"' || c == '\'';
}

bool has_id_boundary_after(std::string_view s, size_t pos) {
  return pos >= s.size() || std::isalnum(static_cast<unsigned char>(s[pos])) == 0;
}

bool matches_steam_uri(std::string_view cmd, std::string_view prefix, std::string_view app_id) {
  size_t pos = 0;
  while ((pos = cmd.find(prefix, pos)) != std::string_view::npos) {
    if (has_token_boundary_before(cmd, pos)) {
      const size_t id_start = pos + prefix.size();
      if (cmd.substr(id_start).starts_with(app_id) &&
          has_id_boundary_after(cmd, id_start + app_id.size())) {
        return true;
      }
    }
    pos += 1;
  }
  return false;
}

bool matches_steam_applaunch(std::string_view cmd, std::string_view app_id) {
  constexpr std::string_view kApplaunchFlag = "-applaunch";
  size_t pos = 0;
  while ((pos = cmd.find(kApplaunchFlag, pos)) != std::string_view::npos) {
    if (has_token_boundary_before(cmd, pos)) {
      size_t id_start = pos + kApplaunchFlag.size();
      if (id_start < cmd.size() && cmd[id_start] == '=') {
        ++id_start;
      } else {
        size_t ws = id_start;
        while (ws < cmd.size() && std::isspace(static_cast<unsigned char>(cmd[ws])) != 0) {
          ++ws;
        }
        if (ws == id_start) {
          pos += 1;
          continue;
        }
        id_start = ws;
      }
      if (cmd.substr(id_start).starts_with(app_id) &&
          has_id_boundary_after(cmd, id_start + app_id.size())) {
        return true;
      }
    }
    pos += 1;
  }
  return false;
}

bool commandline_matches_steam_app_id(std::string_view cmd, std::string_view steam_app_id) {
  return matches_steam_uri(cmd, "steam://rungameid/", steam_app_id) ||
         matches_steam_uri(cmd, "steam://run/", steam_app_id) ||
         matches_steam_applaunch(cmd, steam_app_id);
}

enum class SteamCacheIconKind {
  NONE,
  NAMED_ICON,
  HEX_ICON,
};

struct SteamCacheIconMatch {
  int ext_rank = 0;
  int depth = 0;
  std::uintmax_t file_size = 0;
  std::string path;
};

std::pair<SteamCacheIconKind, int> classify_steam_cache_icon(std::string_view filename) {
  const size_t dot = filename.rfind('.');
  if (dot == std::string_view::npos) {
    return {SteamCacheIconKind::NONE, 0};
  }
  const std::string ext = toLower(std::string(filename.substr(dot)));
  int ext_rank = -1;
  if (ext == ".png") {
    ext_rank = 0;
  } else if (ext == ".jpg") {
    ext_rank = 1;
  } else if (ext == ".jpeg") {
    ext_rank = 2;
  } else if (ext == ".ico") {
    ext_rank = 3;
  } else {
    return {SteamCacheIconKind::NONE, 0};
  }

  const std::string_view stem = filename.substr(0, dot);
  if (ascii_iequals(stem, "icon")) {
    return {SteamCacheIconKind::NAMED_ICON, ext_rank};
  }
  if (stem.size() == 40 &&
      std::ranges::all_of(stem, [](unsigned char c) { return std::isxdigit(c) != 0; })) {
    return {SteamCacheIconKind::HEX_ICON, ext_rank};
  }
  return {SteamCacheIconKind::NONE, 0};
}

std::string find_icon_in_app_dir(const fs::path& app_dir) {
  std::optional<SteamCacheIconMatch> best_named;
  std::optional<SteamCacheIconMatch> best_hex;

  auto consider_file = [&](const fs::directory_entry& entry, int depth) {
    std::error_code entry_ec;
    if (!entry.is_regular_file(entry_ec) || entry_ec) {
      return;
    }
    const auto file_size = entry.file_size(entry_ec);
    if (entry_ec || file_size == 0) {
      return;
    }
    const std::string filename = entry.path().filename().string();
    const auto [kind, ext_rank] = classify_steam_cache_icon(filename);
    if (kind == SteamCacheIconKind::NONE) {
      return;
    }
    const std::string path_str = entry.path().string();
    if (kind == SteamCacheIconKind::NAMED_ICON) {
      if (!best_named || std::tie(depth, ext_rank, path_str) <
                             std::tie(best_named->depth, best_named->ext_rank, best_named->path)) {
        best_named = SteamCacheIconMatch{ext_rank, depth, file_size, path_str};
      }
    } else {
      if (!best_hex ||
          std::tie(file_size, path_str) < std::tie(best_hex->file_size, best_hex->path)) {
        best_hex = SteamCacheIconMatch{ext_rank, depth, file_size, path_str};
      }
    }
  };

  std::error_code ec;
  fs::directory_iterator it(app_dir, fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    return {};
  }
  for (const auto& entry : it) {
    std::error_code entry_ec;
    if (entry.is_directory(entry_ec) && !entry_ec) {
      std::error_code sub_ec;
      fs::directory_iterator sub_it(entry.path(), fs::directory_options::skip_permission_denied,
                                    sub_ec);
      if (!sub_ec) {
        for (const auto& sub_entry : sub_it) {
          consider_file(sub_entry, 1);
        }
      }
    } else {
      consider_file(entry, 0);
    }
  }

  if (best_named) {
    return best_named->path;
  }
  if (best_hex) {
    return best_hex->path;
  }
  return {};
}

}  // namespace

std::optional<std::string> extract_app_id(std::string_view app_id) {
  constexpr std::string_view kSteamAppPrefix = "steam_app_";
  if (app_id.size() <= kSteamAppPrefix.size() ||
      !ascii_iequals(app_id.substr(0, kSteamAppPrefix.size()), kSteamAppPrefix)) {
    return std::nullopt;
  }
  const std::string_view id = app_id.substr(kSteamAppPrefix.size());
  if (id.size() > 20 ||
      !std::ranges::all_of(id, [](unsigned char c) { return c >= '0' && c <= '9'; })) {
    return std::nullopt;
  }
  return std::string(id);
}

bool is_default_app_id(std::string_view app_id) {
  return ascii_iequals(app_id, "steam_app_default");
}

bool is_fallback_app_id(std::string_view app_id) {
  return is_default_app_id(app_id) || extract_app_id(app_id).has_value();
}

bool has_app_id(const std::string& app_id_list) {
  std::string entry;
  std::istringstream stream(app_id_list);
  for (int i = 0; i < 8 && stream >> entry; ++i) {
    if (const auto id = extract_app_id(entry); id && is_nonzero_steam_app_id(*id)) {
      return true;
    }
  }
  return false;
}

bool should_skip_title_fallback(const std::string& app_id_list) {
  std::string entry;
  std::istringstream stream(app_id_list);
  for (int i = 0; i < 8 && stream >> entry; ++i) {
    if (is_default_app_id(entry)) {
      return true;
    }
    if (const auto id = extract_app_id(entry); id && is_nonzero_steam_app_id(*id)) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> extract_fallback_from_list(const std::string& app_id_list) {
  std::string entry;
  std::istringstream stream(app_id_list);
  std::optional<std::string> fallback;
  for (int i = 0; i < 8 && stream >> entry; ++i) {
    if (auto id = extract_app_id(entry)) {
      if (is_nonzero_steam_app_id(*id)) {
        return id;
      }
      if (!fallback) {
        fallback = std::string{};
      }
    } else if (is_default_app_id(entry)) {
      if (!fallback) {
        fallback = std::string{};
      }
    }
  }
  return fallback;
}

Glib::RefPtr<Gio::DesktopAppInfo> find_desktop_app_info(const std::string& steam_app_id) {
  const std::string expected_wm_class = "steam_app_" + steam_app_id;
  const std::string expected_icon = "steam_icon_" + steam_app_id;
  const bool nonzero = is_nonzero_steam_app_id(steam_app_id);

  GList* apps = g_app_info_get_all();
  GDesktopAppInfo* best = nullptr;
  int best_score = 0;
  for (GList* l = apps; l != nullptr; l = l->next) {
    if (!G_IS_DESKTOP_APP_INFO(l->data)) {
      continue;
    }
    auto* info = G_DESKTOP_APP_INFO(l->data);
    int score = 0;
    const auto wm_class = safe_sv(g_desktop_app_info_get_startup_wm_class(info));
    if (!wm_class.empty() && ascii_iequals(wm_class, expected_wm_class)) {
      score = 3;
    } else if (nonzero) {
      const auto cmd = safe_sv(g_app_info_get_commandline(G_APP_INFO(info)));
      if (!cmd.empty() && commandline_matches_steam_app_id(cmd, steam_app_id)) {
        score = 2;
      } else if (GIcon* icon = g_app_info_get_icon(G_APP_INFO(info))) {
        gchar* icon_name = g_icon_to_string(icon);
        const bool icon_match = icon_name != nullptr && safe_sv(icon_name) == expected_icon;
        g_free(icon_name);
        if (icon_match) {
          score = 1;
        }
      }
    }
    if (score > best_score) {
      best = info;
      best_score = score;
      if (best_score >= 3) {
        break;
      }
    }
  }
  Glib::RefPtr<Gio::DesktopAppInfo> match =
      best ? Glib::wrap(best, true) : Glib::RefPtr<Gio::DesktopAppInfo>{};
  g_list_free_full(apps, g_object_unref);
  return match;
}

std::string find_library_icon(const std::string& steam_app_id) {
  if (!is_nonzero_steam_app_id(steam_app_id)) {
    return {};
  }

  std::vector<fs::path> steam_roots;
  auto add_root = [&steam_roots](const fs::path& p) {
    if (!p.empty() && std::ranges::find(steam_roots, p) == steam_roots.end()) {
      steam_roots.push_back(p);
    }
  };

  if (const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
      xdg_data_home != nullptr && xdg_data_home[0] != '\0') {
    add_root(fs::path(xdg_data_home) / "Steam");
  }
  if (const char* home_env = std::getenv("HOME"); home_env != nullptr && home_env[0] != '\0') {
    const fs::path home(home_env);
    add_root(home / ".local/share/Steam");
    add_root(home / ".steam/steam");
    add_root(home / ".steam/root");
    add_root(home / ".var/app/com.valvesoftware.Steam/.local/share/Steam");
    add_root(home / ".var/app/com.valvesoftware.Steam/.steam/steam");
    add_root(home / "snap/steam/common/.local/share/Steam");
  }

  for (const auto& steam_root : steam_roots) {
    const fs::path cache_dir = steam_root / "appcache/librarycache";
    std::error_code ec;
    if (!fs::is_directory(cache_dir, ec) || ec) {
      continue;
    }

    const fs::path app_dir = cache_dir / steam_app_id;
    if (fs::is_directory(app_dir, ec) && !ec) {
      if (std::string icon = find_icon_in_app_dir(app_dir); !icon.empty()) {
        return icon;
      }
    }

    for (const char* ext : {".png", ".jpg", ".jpeg", ".ico"}) {
      const fs::path legacy_icon = cache_dir / (steam_app_id + "_icon" + ext);
      std::error_code file_ec;
      if (fs::is_regular_file(legacy_icon, file_ec) && !file_ec &&
          fs::file_size(legacy_icon, file_ec) > 0 && !file_ec) {
        return legacy_icon.string();
      }
    }
  }

  return {};
}

}  // namespace waybar::util::steam
