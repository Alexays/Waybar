#include "modules/niri/backend.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <thread>

#include "giomm/datainputstream.h"
#include "giomm/dataoutputstream.h"
#include "giomm/unixinputstream.h"
#include "giomm/unixoutputstream.h"

namespace waybar::modules::niri {

int IPC::connectToSocket() {
  const char *socket_path = getenv("NIRI_SOCKET");

  if (socket_path == nullptr) {
    spdlog::warn("Niri is not running, niri IPC will not be available.");
    return -1;
  }

  struct sockaddr_un addr;
  int socketfd = socket(AF_UNIX, SOCK_STREAM, 0);

  if (socketfd == -1) {
    throw std::runtime_error("socketfd failed");
  }

  addr.sun_family = AF_UNIX;

  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

  int l = sizeof(struct sockaddr_un);

  if (connect(socketfd, (struct sockaddr *)&addr, l) == -1) {
    close(socketfd);
    throw std::runtime_error("unable to connect");
  }

  return socketfd;
}

void IPC::startIPC() {
  // will start IPC and relay events to parseIPC

  std::thread([&]() {
    int socketfd;
    try {
      socketfd = connectToSocket();
    } catch (std::exception &e) {
      spdlog::error("Niri IPC: failed to start, reason: {}", e.what());
      return;
    }
    if (socketfd == -1) return;

    spdlog::info("Niri IPC starting");

    auto unix_istream = Gio::UnixInputStream::create(socketfd, true);
    auto unix_ostream = Gio::UnixOutputStream::create(socketfd, false);
    auto istream = Gio::DataInputStream::create(unix_istream);
    auto ostream = Gio::DataOutputStream::create(unix_ostream);

    if (!ostream->put_string("\"EventStream\"\n") || !ostream->flush()) {
      spdlog::error("Niri IPC: failed to start event stream");
      return;
    }

    std::string line;
    if (!istream->read_line(line) || line != R"({"Ok":"Handled"})") {
      spdlog::error("Niri IPC: failed to start event stream");
      return;
    }

    while (istream->read_line(line)) {
      spdlog::debug("Niri IPC: received {}", line);

      try {
        parseIPC(line);
      } catch (std::exception &e) {
        spdlog::warn("Failed to parse IPC message: {}, reason: {}", line, e.what());
      } catch (...) {
        throw;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }).detach();
}

void IPC::parseIPC(const std::string &line) {
  const auto ev = parser_.parse(line);
  const auto members = ev.getMemberNames();
  if (members.size() != 1) throw std::runtime_error("Event must have a single member");

  {
    auto lock = lockData();

    if (const auto &payload = ev["WorkspacesChanged"]) {
      workspaces_.clear();
      const auto &values = payload["workspaces"];
      std::copy(values.begin(), values.end(), std::back_inserter(workspaces_));

      std::sort(workspaces_.begin(), workspaces_.end(), [](const auto &a, const auto &b) {
        const auto &aOutput = a["output"].asString();
        const auto &bOutput = b["output"].asString();
        const auto aIdx = a["idx"].asUInt();
        const auto bIdx = b["idx"].asUInt();
        if (aOutput == bOutput) return aIdx < bIdx;
        return aOutput < bOutput;
      });
    } else if (const auto &payload = ev["WorkspaceActivated"]) {
      const auto id = payload["id"].asUInt64();
      const auto focused = payload["focused"].asBool();
      auto it = std::find_if(workspaces_.begin(), workspaces_.end(),
                             [id](const auto &ws) { return ws["id"].asUInt64() == id; });
      if (it != workspaces_.end()) {
        const auto &ws = *it;
        const auto &output = ws["output"].asString();
        for (auto &ws : workspaces_) {
          const auto got_activated = (ws["id"].asUInt64() == id);
          if (ws["output"] == output) ws["is_active"] = got_activated;

          if (focused) ws["is_focused"] = got_activated;
        }
      } else {
        spdlog::error("Activated unknown workspace");
      }
    } else if (const auto &payload = ev["WorkspaceActiveWindowChanged"]) {
      const auto workspaceId = payload["workspace_id"].asUInt64();
      auto it = std::find_if(workspaces_.begin(), workspaces_.end(), [workspaceId](const auto &ws) {
        return ws["id"].asUInt64() == workspaceId;
      });
      if (it != workspaces_.end()) {
        auto &ws = *it;
        ws["active_window_id"] = payload["active_window_id"];
      } else {
        spdlog::error("Active window changed on unknown workspace");
      }
    } else if (const auto &payload = ev["WorkspaceUrgencyChanged"]) {
      const auto id = payload["id"].asUInt64();
      const auto urgent = payload["urgent"].asBool();
      auto it = std::find_if(workspaces_.begin(), workspaces_.end(),
                             [id](const auto &ws) { return ws["id"].asUInt64() == id; });
      if (it != workspaces_.end()) {
        auto &ws = *it;
        ws["is_urgent"] = urgent;
      } else {
        spdlog::error("Urgency changed for unknown workspace");
      }
    } else if (const auto &payload = ev["KeyboardLayoutsChanged"]) {
      const auto &layouts = payload["keyboard_layouts"];
      const auto &names = layouts["names"];
      keyboardLayoutCurrent_ = layouts["current_idx"].asUInt();

      keyboardLayoutNames_.clear();
      for (const auto &fullName : names) keyboardLayoutNames_.push_back(fullName.asString());
    } else if (const auto &payload = ev["KeyboardLayoutSwitched"]) {
      keyboardLayoutCurrent_ = payload["idx"].asUInt();
    } else if (const auto &payload = ev["WindowsChanged"]) {
      windows_.clear();
      const auto &values = payload["windows"];
      std::copy(values.begin(), values.end(), std::back_inserter(windows_));
    } else if (const auto &payload = ev["WindowOpenedOrChanged"]) {
      const auto &window = payload["window"];
      const auto id = window["id"].asUInt64();
      auto it = std::find_if(windows_.begin(), windows_.end(),
                             [id](const auto &win) { return win["id"].asUInt64() == id; });
      if (it == windows_.end()) {
        windows_.push_back(window);

        if (window["is_focused"].asBool()) {
          for (auto &win : windows_) {
            win["is_focused"] = win["id"].asUInt64() == id;
          }
        }
      } else {
        *it = window;
      }
    } else if (const auto &payload = ev["WindowClosed"]) {
      const auto id = payload["id"].asUInt64();
      auto it = std::find_if(windows_.begin(), windows_.end(),
                             [id](const auto &win) { return win["id"].asUInt64() == id; });
      if (it != windows_.end()) {
        windows_.erase(it);
      } else {
        spdlog::error("Unknown window closed");
      }
    } else if (const auto &payload = ev["WindowFocusChanged"]) {
      const auto focused = !payload["id"].isNull();
      const auto id = payload["id"].asUInt64();
      for (auto &win : windows_) {
        win["is_focused"] = focused && win["id"].asUInt64() == id;
      }
    }
  }

  std::unique_lock lock(callbackMutex_);

  for (auto &[eventname, handler] : callbacks_) {
    if (eventname == members[0]) {
      handler->onEvent(ev);
    }
  }
}

void IPC::registerForIPC(const std::string &ev, EventHandler *ev_handler) {
  if (ev_handler == nullptr) {
    return;
  }

  std::unique_lock lock(callbackMutex_);
  callbacks_.emplace_back(ev, ev_handler);
}

void IPC::unregisterForIPC(EventHandler *ev_handler) {
  if (ev_handler == nullptr) {
    return;
  }

  std::unique_lock lock(callbackMutex_);

  for (auto it = callbacks_.begin(); it != callbacks_.end();) {
    auto &[eventname, handler] = *it;
    if (handler == ev_handler) {
      it = callbacks_.erase(it);
    } else {
      ++it;
    }
  }
}

Json::Value IPC::send(const Json::Value &request) {
  int socketfd = connectToSocket();
  if (socketfd == -1) throw std::runtime_error("Niri is not running");

  auto unix_istream = Gio::UnixInputStream::create(socketfd, true);
  auto unix_ostream = Gio::UnixOutputStream::create(socketfd, false);
  auto istream = Gio::DataInputStream::create(unix_istream);
  auto ostream = Gio::DataOutputStream::create(unix_ostream);

  // Niri needs the request on a single line.
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  std::ostringstream oss;
  writer->write(request, &oss);
  oss << '\n';

  if (!ostream->put_string(oss.str()) || !ostream->flush())
    throw std::runtime_error("error writing to niri socket");

  std::string line;
  if (!istream->read_line(line)) throw std::runtime_error("error reading from niri socket");

  std::istringstream iss(std::move(line));
  Json::Value response;
  iss >> response;
  return response;
}

}  // namespace waybar::modules::niri
