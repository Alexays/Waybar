#include "util/css_reload_helper.hpp"

#include <poll.h>
#include <spdlog/spdlog.h>
#include <sys/inotify.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_map>

#include "config.hpp"
#include "giomm/file.h"
#include "glibmm/refptr.h"

namespace {
const std::regex IMPORT_REGEX(R"(@import\s+(?:url\()?(?:"|')([^"')]+)(?:"|')\)?;)");
}

waybar::CssReloadHelper::CssReloadHelper(std::string cssFile, std::function<void()> callback)
    : m_cssFile(std::move(cssFile)), m_callback(std::move(callback)) {}

std::string waybar::CssReloadHelper::getFileContents(const std::string& filename) {
  if (filename.empty()) {
    return {};
  }

  std::ifstream file(filename);
  if (!file.is_open()) {
    return {};
  }

  return {(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()};
}

std::string waybar::CssReloadHelper::findPath(const std::string& filename) {
  // try path and fallback to looking relative to the config
  std::string result;
  if (std::filesystem::exists(filename)) {
    result = filename;
  } else {
    result = Config::findConfigPath({filename}).value_or("");
  }

  // File monitor does not work with symlinks, so resolve them
  std::string original = result;
  while (std::filesystem::is_symlink(result)) {
    result = std::filesystem::read_symlink(result);

    // prevent infinite cycle
    if (result == original) {
      break;
    }
  }

  return result;
}

void waybar::CssReloadHelper::monitorChanges() {
  auto files = parseImports(m_cssFile);
  for (const auto& file : files) {
    auto gioFile = Gio::File::create_for_path(file);
    if (!gioFile) {
      spdlog::error("Failed to create file for path: {}", file);
      continue;
    }

    auto fileMonitor = gioFile->monitor_file();
    if (!fileMonitor) {
      spdlog::error("Failed to create file monitor for path: {}", file);
      continue;
    }

    auto connection = fileMonitor->signal_changed().connect(
        sigc::mem_fun(*this, &CssReloadHelper::handleFileChange));

    if (!connection.connected()) {
      spdlog::error("Failed to connect to file monitor for path: {}", file);
      continue;
    }
    m_fileMonitors.emplace_back(std::move(fileMonitor));
  }
}

void waybar::CssReloadHelper::handleFileChange(Glib::RefPtr<Gio::File> const& file,
                                               Glib::RefPtr<Gio::File> const& other_type,
                                               Gio::FileMonitorEvent event_type) {
  // Multiple events are fired on file changed (attributes, write, changes done hint, etc.), only
  // fire for one
  if (event_type == Gio::FileMonitorEvent::FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
    spdlog::debug("Reloading style, file changed: {}", file->get_path());
    m_callback();
  }
}

std::vector<std::string> waybar::CssReloadHelper::parseImports(const std::string& cssFile) {
  std::unordered_map<std::string, bool> imports;

  auto cssFullPath = findPath(cssFile);
  if (cssFullPath.empty()) {
    spdlog::error("Failed to find css file: {}", cssFile);
    return {};
  }

  spdlog::debug("Parsing imports for file: {}", cssFullPath);
  imports[cssFullPath] = false;

  auto previousSize = 0UL;
  auto maxIterations = 100U;
  do {
    previousSize = imports.size();
    for (const auto& [file, parsed] : imports) {
      if (!parsed) {
        parseImports(file, imports);
      }
    }

  } while (imports.size() > previousSize && maxIterations-- > 0);

  std::vector<std::string> result;
  for (const auto& [file, parsed] : imports) {
    if (parsed) {
      spdlog::debug("Adding file to watch list: {}", file);
      result.push_back(file);
    }
  }

  return result;
}

void waybar::CssReloadHelper::parseImports(const std::string& cssFile,
                                           std::unordered_map<std::string, bool>& imports) {
  // if the file has already been parsed, skip
  if (imports.find(cssFile) != imports.end() && imports[cssFile]) {
    return;
  }

  auto contents = getFileContents(cssFile);
  std::smatch matches;
  while (std::regex_search(contents, matches, IMPORT_REGEX)) {
    auto importFile = findPath({matches[1].str()});
    if (!importFile.empty() && imports.find(importFile) == imports.end()) {
      imports[importFile] = false;
    }

    contents = matches.suffix().str();
  }

  imports[cssFile] = true;
}
