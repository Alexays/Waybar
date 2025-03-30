#include "modules/hyprland/backend.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <filesystem>
#include <string>

namespace waybar::modules::hyprland {

std::filesystem::path IPC::socketFolder_;

std::filesystem::path IPC::getSocketFolder(const char* instanceSig) {
  static std::mutex folderMutex;
  std::unique_lock lock(folderMutex);

  // socket path, specified by EventManager of Hyprland
  if (!socketFolder_.empty()) {
    return socketFolder_;
  }

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

  socketFolder_ = socketFolder_ / instanceSig;
  return socketFolder_;
}

IPC::IPC() {
  // will start IPC and relay events to parseIPC
  ipcThread_ = std::thread([this]() { socketListener(); });
}

IPC::~IPC() {
  running_ = false;
  spdlog::info("Hyprland IPC stopping...");
  if (socketfd_ != -1) {
    spdlog::trace("Shutting down socket");
    if (shutdown(socketfd_, SHUT_RDWR) == -1) {
      spdlog::error("Hyprland IPC: Couldn't shutdown socket");
    }
    spdlog::trace("Closing socket");
    if (close(socketfd_) == -1) {
      spdlog::error("Hyprland IPC: Couldn't close socket");
    }
  }
  ipcThread_.join();
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

  if (!modulesReady) return;

  spdlog::info("Hyprland IPC starting");

  struct sockaddr_un addr;
  socketfd_ = socket(AF_UNIX, SOCK_STREAM, 0);

  if (socketfd_ == -1) {
    spdlog::error("Hyprland IPC: socketfd failed");
    return;
  }

  addr.sun_family = AF_UNIX;

  auto socketPath = IPC::getSocketFolder(his) / ".socket2.sock";
  strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

  int l = sizeof(struct sockaddr_un);

  if (connect(socketfd_, (struct sockaddr*)&addr, l) == -1) {
    spdlog::error("Hyprland IPC: Unable to connect?");
    return;
  }
  auto* file = fdopen(socketfd_, "r");
  if (file == nullptr) {
    spdlog::error("Hyprland IPC: Couldn't open file descriptor");
    return;
  }
  while (running_) {
    std::array<char, 1024> buffer;  // Hyprland socket2 events are max 1024 bytes

    auto* receivedCharPtr = fgets(buffer.data(), buffer.size(), file);

    if (receivedCharPtr == nullptr) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    std::string messageReceived(buffer.data());
    messageReceived = messageReceived.substr(0, messageReceived.find_first_of('\n'));
    spdlog::debug("hyprland IPC received {}", messageReceived);

    try {
      parseIPC(messageReceived);
    } catch (std::exception& e) {
      spdlog::warn("Failed to parse IPC message: {}, reason: {}", messageReceived, e.what());
    } catch (...) {
      throw;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

  const auto serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);

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
  if (snprintf(serverAddress.sun_path, sizeof(serverAddress.sun_path), "%s", socketPath.c_str()) <
      0) {
    throw std::runtime_error("Hyprland IPC: Couldn't copy socket path (6)");
  }

  if (connect(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) <
      0) {
    throw std::runtime_error("Hyprland IPC: Couldn't connect to " + socketPath + ". (3)");
  }

  auto sizeWritten = write(serverSocket, rq.c_str(), rq.length());

  if (sizeWritten < 0) {
    spdlog::error("Hyprland IPC: Couldn't write (4)");
    return "";
  }

  std::array<char, 8192> buffer = {0};
  std::string response;

  do {
    sizeWritten = read(serverSocket, buffer.data(), 8192);

    if (sizeWritten < 0) {
      spdlog::error("Hyprland IPC: Couldn't read (5)");
      close(serverSocket);
      return "";
    }
    response.append(buffer.data(), sizeWritten);
  } while (sizeWritten > 0);

  close(serverSocket);
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
