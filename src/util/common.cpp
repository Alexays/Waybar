#include "util/common.hpp"

#include <spdlog/spdlog.h>

#include "util/string.hpp"

namespace waybar::util {

std::vector<std::string> search_prefix() {
  std::vector<std::string> prefixes = {""};

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

}  // namespace waybar::util
