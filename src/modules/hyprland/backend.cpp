#include "modules/hyprland/backend.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>
#include <thread>

namespace waybar::modules::hyprland {

void IPC::startIPC() {
  // will start IPC and relay events to parseIPC

  std::thread([&]() {
    // check for hyprland
    const char* his = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (his == nullptr) {
      spdlog::warn("Hyprland is not running, Hyprland IPC will not be available.");
      return;
    }

    if (!modulesReady) return;

    spdlog::info("Hyprland IPC starting");

    struct sockaddr_un addr;
    int socketfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (socketfd == -1) {
      spdlog::error("Hyprland IPC: socketfd failed");
      return;
    }

    addr.sun_family = AF_UNIX;

    // socket path, specified by EventManager of Hyprland
    std::string socketPath = "/tmp/hypr/" + std::string(his) + "/.socket2.sock";

    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    int l = sizeof(struct sockaddr_un);

    if (connect(socketfd, (struct sockaddr*)&addr, l) == -1) {
      spdlog::error("Hyprland IPC: Unable to connect?");
      return;
    }

    auto file = fdopen(socketfd, "r");

    while (true) {
      char buffer[1024];  // Hyprland socket2 events are max 1024 bytes

      auto recievedCharPtr = fgets(buffer, 1024, file);

      if (!recievedCharPtr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      std::string messageRecieved(buffer);
      messageRecieved = messageRecieved.substr(0, messageRecieved.find_first_of('\n'));
      spdlog::debug("hyprland IPC received {}", messageRecieved);
      parseIPC(messageRecieved);

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }).detach();
}

void IPC::parseIPC(const std::string& ev) {
  std::string request = ev.substr(0, ev.find_first_of('>'));
  std::unique_lock lock(m_callbackMutex);

  for (auto& [eventname, handler] : m_callbacks) {
    if (eventname == request) {
      handler->onEvent(ev);
    }
  }
}

void IPC::registerForIPC(const std::string& ev, EventHandler* ev_handler) {
  if (!ev_handler) {
    return;
  }

  std::unique_lock lock(m_callbackMutex);
  m_callbacks.emplace_back(ev, ev_handler);
}

void IPC::unregisterForIPC(EventHandler* ev_handler) {
  if (!ev_handler) {
    return;
  }

  std::unique_lock lock(m_callbackMutex);

  for (auto it = m_callbacks.begin(); it != m_callbacks.end();) {
    auto& [eventname, handler] = *it;
    if (handler == ev_handler) {
      m_callbacks.erase(it++);
    } else {
      ++it;
    }
  }
}

std::string IPC::getSocket1Reply(const std::string& rq) {
  // basically hyprctl

  struct addrinfo aiHints;
  struct addrinfo* aiRes = nullptr;
  const auto serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);

  if (serverSocket < 0) {
    spdlog::error("Hyprland IPC: Couldn't open a socket (1)");
    return "";
  }

  memset(&aiHints, 0, sizeof(struct addrinfo));
  aiHints.ai_family = AF_UNSPEC;
  aiHints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo("localhost", nullptr, &aiHints, &aiRes) != 0) {
    spdlog::error("Hyprland IPC: Couldn't get host (2)");
    return "";
  }

  // get the instance signature
  auto instanceSig = getenv("HYPRLAND_INSTANCE_SIGNATURE");

  if (!instanceSig) {
    spdlog::error("Hyprland IPC: HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)");
    return "";
  }

  std::string instanceSigStr = std::string(instanceSig);

  sockaddr_un serverAddress = {0};
  serverAddress.sun_family = AF_UNIX;

  std::string socketPath = "/tmp/hypr/" + instanceSigStr + "/.socket.sock";

  // Use snprintf to copy the socketPath string into serverAddress.sun_path
  if (snprintf(serverAddress.sun_path, sizeof(serverAddress.sun_path), "%s", socketPath.c_str()) <
      0) {
    spdlog::error("Hyprland IPC: Couldn't copy socket path (6)");
    return "";
  }

  if (connect(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) <
      0) {
    spdlog::error("Hyprland IPC: Couldn't connect to " + socketPath + ". (3)");
    return "";
  }

  auto sizeWritten = write(serverSocket, rq.c_str(), rq.length());

  if (sizeWritten < 0) {
    spdlog::error("Hyprland IPC: Couldn't write (4)");
    return "";
  }

  char buffer[8192] = {0};
  std::string response;

  do {
    sizeWritten = read(serverSocket, buffer, 8192);

    if (sizeWritten < 0) {
      spdlog::error("Hyprland IPC: Couldn't read (5)");
      close(serverSocket);
      return "";
    }
    response.append(buffer, sizeWritten);
  } while (sizeWritten > 0);

  close(serverSocket);
  return response;
}

Json::Value IPC::getSocket1JsonReply(const std::string& rq) {
  return m_parser.parse(getSocket1Reply("j/" + rq));
}

}  // namespace waybar::modules::hyprland
