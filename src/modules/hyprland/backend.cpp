#include "modules/hyprland/backend.hpp"

#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>

namespace waybar::modules::hyprland {

void IPC::startIPC() {
  // will start IPC and relay events to parseIPC

  std::thread([&]() {
    // check for hyprland
    const char* HIS = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (!HIS) {
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

    // socket path
    std::string socketPath = "/tmp/hypr/" + std::string(HIS) + "/.socket2.sock";

    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    int l = sizeof(struct sockaddr_un);

    if (connect(socketfd, (struct sockaddr*)&addr, l) == -1) {
      spdlog::error("Hyprland IPC: Unable to connect?");
      return;
    }

    auto file = fdopen(socketfd, "r");

    while (1) {
      // read

      char buffer[1024];  // Hyprland socket2 events are max 1024 bytes
      auto recievedCharPtr = fgets(buffer, 1024, file);

      if (!recievedCharPtr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      callbackMutex.lock();

      std::string messageRecieved(buffer);

      messageRecieved = messageRecieved.substr(0, messageRecieved.find_first_of('\n'));

      spdlog::debug("hyprland IPC received {}", messageRecieved);

      parseIPC(messageRecieved);

      callbackMutex.unlock();

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }).detach();
}

void IPC::parseIPC(const std::string& ev) {
  // todo
  std::string request = ev.substr(0, ev.find_first_of('>'));

  for (auto& [eventname, handler] : callbacks) {
    if (eventname == request) {
      handler->onEvent(ev);
    }
  }
}

void IPC::registerForIPC(const std::string& ev, EventHandler* ev_handler) {
  if (!ev_handler) {
    return;
  }
  callbackMutex.lock();

  callbacks.emplace_back(std::make_pair(ev, ev_handler));

  callbackMutex.unlock();
}

void IPC::unregisterForIPC(EventHandler* ev_handler) {
  if (!ev_handler) {
    return;
  }

  callbackMutex.lock();

  for (auto it = callbacks.begin(); it != callbacks.end();) {
    auto it_current = it;
    it++;
    auto& [eventname, handler] = *it_current;
    if (handler == ev_handler) {
      callbacks.erase(it_current);
    }
  }

  callbackMutex.unlock();
}

std::string IPC::getSocket1Reply(const std::string& rq) {
  // basically hyprctl

  const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

  if (SERVERSOCKET < 0) {
    spdlog::error("Hyprland IPC: Couldn't open a socket (1)");
    return "";
  }

  const auto SERVER = gethostbyname("localhost");

  if (!SERVER) {
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

  strcpy(serverAddress.sun_path, socketPath.c_str());

  if (connect(SERVERSOCKET, (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)) < 0) {
    spdlog::error("Hyprland IPC: Couldn't connect to " + socketPath + ". (3)");
    return "";
  }

  auto sizeWritten = write(SERVERSOCKET, rq.c_str(), rq.length());

  if (sizeWritten < 0) {
    spdlog::error("Hyprland IPC: Couldn't write (4)");
    return "";
  }

  char buffer[8192] = {0};

  sizeWritten = read(SERVERSOCKET, buffer, 8192);

  if (sizeWritten < 0) {
    spdlog::error("Hyprland IPC: Couldn't read (5)");
    return "";
  }

  close(SERVERSOCKET);

  return std::string(buffer);
}

}  // namespace waybar::modules::hyprland
