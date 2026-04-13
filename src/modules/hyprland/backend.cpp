#include "modules/hyprland/backend.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <string>

#include "util/scoped_fd.hpp"

namespace waybar::modules::hyprland {

std::filesystem::path IPC::socketFolder_;

std::filesystem::path IPC::getSocketFolder(const char* instanceSig) {
  static std::mutex folderMutex;
  std::unique_lock lock(folderMutex);

  if (socketFolder_.empty()) {
    const char* xdgRuntimeDirEnv = std::getenv("XDG_RUNTIME_DIR");
    std::filesystem::path xdgRuntimeDir;
    // Only set path if env variable is set
    if (xdgRuntimeDirEnv != nullptr) {
      xdgRuntimeDir = std::filesystem::path(xdgRuntimeDirEnv);
    }

    if (!xdgRuntimeDir.empty() && std::filesystem::exists(xdgRuntimeDir / "hypr")) {
      socketFolder_ = xdgRuntimeDir / "hypr";
    } else {
      spdlog::warn("$XDG_RUNTIME_DIR/hypr does not exist, falling back to /tmp/hypr");
      socketFolder_ = std::filesystem::path("/tmp") / "hypr";
    }
  }

  return socketFolder_ / instanceSig;
}

IPC::IPC() {
  // will start IPC and relay events to parseIPC
  socketOwnerPid_ = getpid();
  ipcThread_ = std::thread([this]() { socketListener(); });
}

IPC::~IPC() {
  // Do no stop Hyprland IPC if a child process (with successful fork() but
  // failed exec()) exits.
  if (getpid() != socketOwnerPid_) return;

  running_.store(false, std::memory_order_relaxed);
  spdlog::info("Hyprland IPC stopping...");
  {
    std::lock_guard<std::mutex> lock(socketMutex_);
    if (socketfd_ != -1) {
      spdlog::trace("Shutting down socket");
      if (shutdown(socketfd_, SHUT_RDWR) == -1 && errno != ENOTCONN) {
        spdlog::error("Hyprland IPC: Couldn't shutdown socket");
      }
    }
  }
  if (ipcThread_.joinable()) {
    ipcThread_.join();
  }
}

IPC& IPC::inst() {
  static IPC ipc;
  return ipc;
}

void IPC::socketListener() {
  // check for hyprland
  const char* his = getenv("HYPRLAND_INSTANCE_SIGNATURE");

  if (his == nullptr) {
    spdlog::warn("Hyprland is not running, Hyprland IPC will not be available.");
    return;
  }

  spdlog::info("Hyprland IPC starting");

  struct sockaddr_un addr = {};
  const int socketfd = socket(AF_UNIX, SOCK_STREAM, 0);

  if (socketfd == -1) {
    spdlog::error("Hyprland IPC: socketfd failed");
    return;
  }

  addr.sun_family = AF_UNIX;

  auto socketPath = IPC::getSocketFolder(his) / ".socket2.sock";
  if (socketPath.native().size() >= sizeof(addr.sun_path)) {
    spdlog::error("Hyprland IPC: Socket path is too long: {}", socketPath.string());
    close(socketfd);
    return;
  }
  strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

  int l = sizeof(struct sockaddr_un);

  if (connect(socketfd, (struct sockaddr*)&addr, l) == -1) {
    spdlog::error("Hyprland IPC: Unable to connect? {}", std::strerror(errno));
    close(socketfd);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(socketMutex_);
    socketfd_ = socketfd;
  }

  std::string pending;
  while (running_.load(std::memory_order_relaxed)) {
    std::array<char, 1024> buffer;  // Hyprland socket2 events are max 1024 bytes
    const ssize_t bytes_read = read(socketfd, buffer.data(), buffer.size());

    if (bytes_read == 0) {
      if (running_.load(std::memory_order_relaxed)) {
        spdlog::warn("Hyprland IPC: Socket closed by peer");
      }
      break;
    }

    if (bytes_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (!running_.load(std::memory_order_relaxed)) {
        break;
      }
      spdlog::error("Hyprland IPC: read failed: {}", std::strerror(errno));
      break;
    }

    pending.append(buffer.data(), static_cast<std::size_t>(bytes_read));
    for (auto newline_pos = pending.find('\n'); newline_pos != std::string::npos;
         newline_pos = pending.find('\n')) {
      std::string messageReceived = pending.substr(0, newline_pos);
      pending.erase(0, newline_pos + 1);
      if (messageReceived.empty()) {
        continue;
      }
      spdlog::debug("hyprland IPC received {}", messageReceived);

      try {
        parseIPC(messageReceived);
      } catch (std::exception& e) {
        spdlog::warn("Failed to parse IPC message: {}, reason: {}", messageReceived, e.what());
      } catch (...) {
        throw;
      }
    }
  }
  {
    std::lock_guard<std::mutex> lock(socketMutex_);
    if (socketfd_ != -1) {
      if (close(socketfd_) == -1) {
        spdlog::error("Hyprland IPC: Couldn't close socket");
      }
      socketfd_ = -1;
    }
  }
  spdlog::debug("Hyprland IPC stopped");
}

void IPC::parseIPC(const std::string& ev) {
  std::string request = ev.substr(0, ev.find_first_of('>'));
  std::unique_lock lock(callbackMutex_);

  for (auto& [eventname, handler] : callbacks_) {
    if (eventname == request) {
      handler->onEvent(ev);
    }
  }
}

void IPC::registerForIPC(const std::string& ev, EventHandler* ev_handler) {
  if (ev_handler == nullptr) {
    return;
  }

  std::unique_lock lock(callbackMutex_);
  callbacks_.emplace_back(ev, ev_handler);
}

void IPC::unregisterForIPC(EventHandler* ev_handler) {
  if (ev_handler == nullptr) {
    return;
  }

  std::unique_lock lock(callbackMutex_);

  for (auto it = callbacks_.begin(); it != callbacks_.end();) {
    auto& [eventname, handler] = *it;
    if (handler == ev_handler) {
      callbacks_.erase(it++);
    } else {
      ++it;
    }
  }
}

std::string IPC::getSocket1Reply(const std::string& rq) {
  // basically hyprctl

  util::ScopedFd serverSocket(socket(AF_UNIX, SOCK_STREAM, 0));

  if (serverSocket < 0) {
    throw std::runtime_error("Hyprland IPC: Couldn't open a socket (1)");
  }

  // get the instance signature
  auto* instanceSig = getenv("HYPRLAND_INSTANCE_SIGNATURE");

  if (instanceSig == nullptr) {
    throw std::runtime_error(
        "Hyprland IPC: HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)");
  }

  sockaddr_un serverAddress = {0};
  serverAddress.sun_family = AF_UNIX;

  std::string socketPath = IPC::getSocketFolder(instanceSig) / ".socket.sock";

  // Use snprintf to copy the socketPath string into serverAddress.sun_path
  const auto socketPathLength =
      snprintf(serverAddress.sun_path, sizeof(serverAddress.sun_path), "%s", socketPath.c_str());
  if (socketPathLength < 0 ||
      socketPathLength >= static_cast<int>(sizeof(serverAddress.sun_path))) {
    throw std::runtime_error("Hyprland IPC: Couldn't copy socket path (6)");
  }

  if (connect(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) <
      0) {
    throw std::runtime_error("Hyprland IPC: Couldn't connect to " + socketPath + ". (3)");
  }

  std::size_t totalWritten = 0;
  while (totalWritten < rq.length()) {
    const auto sizeWritten =
        write(serverSocket, rq.c_str() + totalWritten, rq.length() - totalWritten);

    if (sizeWritten < 0) {
      if (errno == EINTR) {
        continue;
      }
      spdlog::error("Hyprland IPC: Couldn't write (4)");
      return "";
    }
    if (sizeWritten == 0) {
      spdlog::error("Hyprland IPC: Socket write made no progress");
      return "";
    }
    totalWritten += static_cast<std::size_t>(sizeWritten);
  }

  std::array<char, 8192> buffer = {0};
  std::string response;
  ssize_t sizeWritten = 0;

  do {
    sizeWritten = read(serverSocket, buffer.data(), 8192);

    if (sizeWritten < 0) {
      spdlog::error("Hyprland IPC: Couldn't read (5)");
      return "";
    }
    response.append(buffer.data(), sizeWritten);
  } while (sizeWritten > 0);

  return response;
}

Json::Value IPC::getSocket1JsonReply(const std::string& rq) {
  std::string reply = getSocket1Reply("j/" + rq);

  if (reply.empty()) {
    return {};
  }

  return parser_.parse(reply);
}

}  // namespace waybar::modules::hyprland
