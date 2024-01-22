#include "util/css_reload_helper.hpp"

#include <poll.h>
#include <spdlog/spdlog.h>
#include <sys/inotify.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_map>

#include "config.hpp"
namespace {
const std::regex IMPORT_REGEX(R"(@import\s+(?:url\()?(?:"|')([^"')]+)(?:"|')\)?;)");
}

waybar::CssReloadHelper::CssReloadHelper(std::string cssFile, std::function<void()> callback)
    : m_cssFile(std::move(cssFile)), m_callback(std::move(callback)) {}

waybar::CssReloadHelper::~CssReloadHelper() { stop(); }

std::string waybar::CssReloadHelper::getFileContents(const std::string& filename) {
  if (filename.empty()) {
    return {};
  }

  std::ifstream file(filename);
  if (!file.is_open()) {
    return {};
  }

  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string waybar::CssReloadHelper::findPath(const std::string& filename) {
  // try path and fallback to looking relative to the config
  if (std::filesystem::exists(filename)) {
    return filename;
  }

  return Config::findConfigPath({filename}).value_or("");
}

void waybar::CssReloadHelper::monitorChanges() {
  m_thread = std::thread([this] {
    m_running = true;
    while (m_running) {
      auto files = parseImports(m_cssFile);
      watchFiles(files);
    }
  });
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

void waybar::CssReloadHelper::stop() {
  if (!m_running) {
    return;
  }

  m_running = false;
  m_cv.notify_all();
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void waybar::CssReloadHelper::watchFiles(const std::vector<std::string>& files) {
  auto inotifyFd = inotify_init1(IN_NONBLOCK);
  if (inotifyFd < 0) {
    spdlog::error("Failed to initialize inotify: {}", strerror(errno));
    return;
  }

  std::vector<int> watchFds;
  for (const auto& file : files) {
    auto watchFd = inotify_add_watch(inotifyFd, file.c_str(), IN_MODIFY | IN_MOVED_TO);
    if (watchFd < 0) {
      spdlog::error("Failed to add watch for file: {}", file);
    } else {
      spdlog::debug("Added watch for file: {}", file);
    }
    watchFds.push_back(watchFd);
  }

  auto pollFd = pollfd{inotifyFd, POLLIN, 0};

  while (true) {
    if (watch(inotifyFd, &pollFd)) {
      break;
    }
  }

  for (const auto& watchFd : watchFds) {
    inotify_rm_watch(inotifyFd, watchFd);
  }

  close(inotifyFd);
}

bool waybar::CssReloadHelper::watch(int inotifyFd, pollfd* pollFd) {
  auto pollResult = poll(pollFd, 1, 10);
  if (pollResult < 0) {
    spdlog::error("Failed to poll inotify: {}", strerror(errno));
    return true;
  }

  if (pollResult == 0) {
    // check if we should stop
    if (!m_running) {
      return true;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    // a condition variable is used to allow the thread to be stopped immediately while still not
    // spamming poll
    m_cv.wait_for(lock, std::chrono::milliseconds(250), [this] { return !m_running; });

    // timeout
    return false;
  }

  if (static_cast<bool>(pollFd->revents & POLLIN)) {
    if (handleInotifyEvents(inotifyFd)) {
      // after the callback is fired we need to re-parse the imports and setup the watches
      // again in case the import list has changed
      return true;
    }
  }

  return false;
}

bool waybar::CssReloadHelper::handleInotifyEvents(int inotify_fd) {
  // inotify event
  auto buffer = std::array<char, 4096>{};
  auto readResult = read(inotify_fd, buffer.data(), buffer.size());
  if (readResult < 0) {
    spdlog::error("Failed to read inotify event: {}", strerror(errno));
    return false;
  }

  auto offset = 0;
  auto shouldFireCallback = false;

  // read all events on the fd
  while (offset < readResult) {
    auto* event = reinterpret_cast<inotify_event*>(buffer.data() + offset);
    offset += sizeof(inotify_event) + event->len;
    shouldFireCallback = true;
  }

  // we only need to fire the callback once
  if (shouldFireCallback) {
    m_callback();
    return true;
  }

  return false;
}
