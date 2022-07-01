#include "modules/hyprland/backend.hpp"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

void waybar::modules::hyprland::IPC::startIPC() {
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

void waybar::modules::hyprland::IPC::parseIPC(const std::string& ev) {
  // todo
  std::string request = ev.substr(0, ev.find_first_of('>'));

  for (auto& [eventname, handler] : callbacks) {
    if (eventname == request) {
      handler(ev);
    }
  }
}

void waybar::modules::hyprland::IPC::registerForIPC(const std::string& ev,
                                                    std::function<void(const std::string&)> fn) {
  callbackMutex.lock();

  callbacks.emplace_back(std::make_pair(ev, fn));

  callbackMutex.unlock();
}