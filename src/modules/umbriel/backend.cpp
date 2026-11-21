#include "modules/umbriel/backend.hpp"

#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>

#include "giomm/datainputstream.h"
#include "giomm/dataoutputstream.h"
#include "giomm/unixinputstream.h"
#include "giomm/unixoutputstream.h"
#include "util/scoped_fd.hpp"

namespace waybar::modules::umbriel {

std::string IPC::socketPath() {
  const char* socket_path = std::getenv("UMBRIEL_SOCKET");
  if (socket_path != nullptr && socket_path[0] != '\0') {
    return socket_path;
  }

  const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
  const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
  if (runtime_dir != nullptr && runtime_dir[0] != '\0' && wayland_display != nullptr &&
      wayland_display[0] != '\0') {
    return std::string(runtime_dir) + "/umbriel-" + wayland_display + ".sock";
  }

  throw std::runtime_error(
      "Umbriel IPC: UMBRIEL_SOCKET is not set and WAYLAND_DISPLAY/XDG_RUNTIME_DIR are "
      "unavailable (is Umbriel running?)");
}

int IPC::connectToSocket() {
  const auto path = socketPath();

  struct sockaddr_un addr{};
  util::ScopedFd socketfd(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (socketfd == -1) {
    throw std::runtime_error("Umbriel IPC: socket() failed: " + std::string(strerror(errno)));
  }

  addr.sun_family = AF_UNIX;
  if (path.size() >= sizeof(addr.sun_path)) {
    throw std::runtime_error("Umbriel IPC: socket path is too long: " + path);
  }
  std::memcpy(addr.sun_path, path.data(), path.size());

  if (connect(socketfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
    throw std::runtime_error("Umbriel IPC: unable to connect to " + path + ": " +
                             std::string(strerror(errno)));
  }

  return socketfd.release();
}

std::string IPC::serialize(const Json::Value& request) {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

  std::ostringstream oss;
  writer->write(request, &oss);
  oss << '\n';
  return oss.str();
}

IPC::IPC() {
  // Connect synchronously so a missing socket (this WM isn't the active
  // compositor) throws here. That lets the module constructor fail and Factory
  // disable the module instead of attaching a permanently empty widget.
  startIPC(connectToSocket());
}

IPC::~IPC() { running_ = false; }

void IPC::startIPC(int initial_socketfd) {
  std::thread([this, initial_socketfd]() {
    spdlog::info("Umbriel IPC starting");

    bool have_initial_fd = true;

    // Reconnect loop: the initial connect above already proved the socket is
    // reachable. If the event stream drops later, reconnect instead of leaving
    // every module frozen with stale state.
    while (running_) {
      int socketfd;
      if (have_initial_fd) {
        socketfd = initial_socketfd;
        have_initial_fd = false;
      } else {
        try {
          socketfd = connectToSocket();
        } catch (const std::exception& e) {
          spdlog::error("Umbriel IPC: failed to reconnect: {}", e.what());
          std::this_thread::sleep_for(std::chrono::seconds(2));
          continue;
        }
      }

      auto unix_istream = Gio::UnixInputStream::create(socketfd, true);
      auto unix_ostream = Gio::UnixOutputStream::create(socketfd, false);
      auto istream = Gio::DataInputStream::create(unix_istream);
      auto ostream = Gio::DataOutputStream::create(unix_ostream);

      Json::Value request(Json::objectValue);
      request["cmd"] = "subscribe";
      request["events"] = Json::Value(Json::arrayValue);
      request["events"].append("workspaces");
      request["events"].append("windows");

      if (!ostream->put_string(serialize(request)) || !ostream->flush()) {
        spdlog::error("Umbriel IPC: failed to start event stream");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      }

      std::string line;
      // Umbriel answers a subscribe with the current snapshot of every
      // requested family, then one fresh snapshot whenever that family
      // changes. There is no separate acknowledgement line to consume.
      while (running_ && istream->read_line(line)) {
        spdlog::debug("Umbriel IPC: received {}", line);

        try {
          parseIPC(line);
        } catch (const std::exception& e) {
          spdlog::warn("Umbriel IPC: failed to parse message: {}, reason: {}", line, e.what());
        } catch (...) {
          throw;
        }
      }

      if (!running_) break;

      spdlog::warn("Umbriel IPC: event stream closed, reconnecting");
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    spdlog::info("Umbriel IPC stopping");
  }).detach();
}

void IPC::parseIPC(const std::string& line) {
  const auto ev = parser_.parse(line);

  if (!ev.isObject()) {
    throw std::runtime_error("Event is not a JSON object");
  }

  if (ev.isMember("err")) {
    throw std::runtime_error("Umbriel IPC error: " + ev["err"].asString());
  }

  const auto& event_value = ev["event"];
  if (!event_value.isString()) {
    throw std::runtime_error("Event has no name");
  }

  const auto event = event_value.asString();
  const auto& data = ev["data"];

  {
    auto lock = lockData();

    if (event == "workspaces" && data.isArray()) {
      workspaces_.assign(data.begin(), data.end());
    } else if (event == "windows" && data.isArray()) {
      windows_.assign(data.begin(), data.end());
    }
  }

  std::unique_lock lock(callbackMutex_);
  for (auto& [eventname, handler] : callbacks_) {
    if (eventname == event) {
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
    if (it->second == ev_handler) {
      it = callbacks_.erase(it);
    } else {
      ++it;
    }
  }
}

Json::Value IPC::send(const Json::Value& request) {
  util::ScopedFd socketfd(connectToSocket());

  auto unix_istream = Gio::UnixInputStream::create(socketfd, false);
  auto unix_ostream = Gio::UnixOutputStream::create(socketfd, false);
  auto istream = Gio::DataInputStream::create(unix_istream);
  auto ostream = Gio::DataOutputStream::create(unix_ostream);

  if (!ostream->put_string(serialize(request)) || !ostream->flush()) {
    throw std::runtime_error("Umbriel IPC: error writing request");
  }

  std::string line;
  if (!istream->read_line(line)) {
    throw std::runtime_error("Umbriel IPC: error reading reply");
  }

  util::JsonParser parser;
  auto reply = parser.parse(line);
  if (reply.isMember("err")) {
    throw std::runtime_error("Umbriel IPC: " + reply["err"].asString());
  }
  return reply;
}

}  // namespace waybar::modules::umbriel
