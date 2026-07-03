#include "modules/mango/backend.hpp"

#include <fcntl.h>
#include <poll.h>
#include <spdlog/spdlog.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <sstream>
#include <thread>
#include <vector>

#include "util/scoped_fd.hpp"

namespace waybar::modules::mango {

int IPC::connectToSocket() {
  const char* socket_path = getenv("MANGO_INSTANCE_SIGNATURE");
  if (!socket_path) {
    throw std::runtime_error("Mango IPC: MANGO_INSTANCE_SIGNATURE not set");
  }

  struct sockaddr_un addr;
  util::ScopedFd fd(socket(AF_UNIX, SOCK_STREAM, 0));
  if (fd == -1) throw std::runtime_error("socket() failed");

  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    throw std::runtime_error("connect() failed");
  }
  return fd.release();
}

Json::Value IPC::sendCommand(const std::string& cmd) {
  util::ScopedFd fd(IPC::connectToSocket());
  std::string full_cmd = cmd + "\n";

  ssize_t total_written = 0;
  while (total_written < (ssize_t)full_cmd.size()) {
    ssize_t res = write(fd, full_cmd.c_str() + total_written, full_cmd.size() - total_written);
    if (res < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("Failed to write command");
    }
    total_written += res;
  }

  char buf[4096];
  std::string response;
  while (true) {
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
      if (n == 0) break;
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
      throw std::runtime_error("Read error");
    }
    buf[n] = '\0';
    response += buf;
    if (response.find('\n') != std::string::npos) break;
  }

  Json::Value root;
  std::istringstream iss(response);
  Json::CharReaderBuilder builder;
  std::string errors;
  if (!Json::parseFromStream(builder, iss, &root, &errors)) {
    throw std::runtime_error("JSON parse error: " + errors);
  }
  return root;
}

Json::Value IPC::send(const Json::Value& request) {
  if (!request.isMember("command")) {
    throw std::runtime_error("Mango IPC: request must have 'command' field");
  }
  return sendCommand(request["command"].asString());
}

void IPC::sendAsync(const Json::Value& request) {
  if (!request.isMember("command")) {
    spdlog::error("Mango IPC: request must have 'command' field");
    return;
  }
  std::string cmd = request["command"].asString();

  std::thread([cmd]() {
    try {
      IPC::sendCommand(cmd);
    } catch (const std::exception& e) {
      spdlog::error("IPC async send failed: {}", e.what());
    }
  }).detach();
}

IPC::IPC() : sockfd_(-1), active_client_(Json::nullValue) { startIPC(); }

IPC::~IPC() {
  if (sockfd_ != -1) close(sockfd_);
  if (ipc_thread_.joinable()) ipc_thread_.join();
}

void IPC::startIPC() {
  sockfd_ = IPC::connectToSocket();

  ipc_thread_ = std::thread([this]() {
    spdlog::info("Mango IPC thread started");

    struct pollfd pfd;
    pfd.fd = sockfd_;
    pfd.events = POLLIN;

    const std::vector<std::string> subs = {"watch all-monitors"};
    for (const auto& cmd : subs) {
      if (write(sockfd_, cmd.c_str(), cmd.size()) != (ssize_t)cmd.size() ||
          write(sockfd_, "\n", 1) != 1) {
        spdlog::error("Failed to subscribe to {}", cmd);
        return;
      }
    }

    char buf[4096];
    std::string buffer;
    while (true) {
      int ret = poll(&pfd, 1, 1000);
      if (ret == 0) continue;
      if (ret < 0) {
        if (errno == EINTR) continue;
        spdlog::error("IPC poll error: {}", strerror(errno));
        break;
      }

      if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        spdlog::info("Mango IPC socket closed or invalid");
        break;
      }

      if (pfd.revents & POLLIN) {
        ssize_t n = read(sockfd_, buf, sizeof(buf));
        if (n == 0) {
          spdlog::info("Mango IPC connection closed");
          break;
        }
        if (n < 0) {
          if (errno == EINTR) continue;
          spdlog::error("IPC read error: {}", strerror(errno));
          break;
        }
        buffer.append(buf, n);

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
          std::string line = buffer.substr(0, pos);
          buffer.erase(0, pos + 1);
          if (line.empty()) continue;
          try {
            parseIPC(line);
          } catch (const std::exception& e) {
            spdlog::warn("Failed to parse IPC line: {} - {}", line, e.what());
          }
        }
      }
    }
  });
}

void IPC::parseIPC(const std::string& line) {
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::string errors;
  std::istringstream iss(line);
  if (!Json::parseFromStream(builder, iss, &root, &errors)) {
    throw std::runtime_error("JSON parse error: " + errors);
  }

  if (root.isMember("monitors") && root["monitors"].isArray()) {
    for (const auto& mon : root["monitors"]) {
      handleMonitorUpdate(mon);
    }

    Json::Value active_monitor;
    for (const auto& mon : root["monitors"]) {
      if (mon["active"].asBool()) {
        active_monitor = mon;
        break;
      }
    }

    if (!active_monitor.isNull()) {
      const auto& active_client = active_monitor["active_client"];
      updateFocusingClient(active_client);

      if (active_monitor.isMember("keyboardlayout")) {
        updateKeyboardLayout(active_monitor["keyboardlayout"].asString());
      }

      if (active_monitor.isMember("keymode")) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        keymode_ = active_monitor["keymode"].asString();
      }
    }

    std::vector<EventHandler*> handlers_to_notify;
    {
      std::lock_guard<std::mutex> lock(callback_mutex_);
      for (auto& [ev, handler] : callbacks_) {
        if (ev == "monitor") {
          handlers_to_notify.push_back(handler);
        }
      }
    }

    for (auto* handler : handlers_to_notify) {
      handler->onEvent(root);
    }

    return;
  }

  spdlog::debug("Unhandled IPC message: {}", line);
}

std::unordered_map<std::string, Json::Value> IPC::getMonitors() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return monitors_;
}

IPC& IPC::getInstance() {
  static IPC instance;
  return instance;
}

Json::Value IPC::getMonitor(const std::string& name) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  auto it = monitors_.find(name);
  if (it != monitors_.end()) {
    return it->second;
  }
  return Json::nullValue;
}

std::string IPC::getKeyboardLayout() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return keyboard_layout_;
}

std::string IPC::getKeymode() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return keymode_;
}

Json::Value IPC::getActiveClientForMonitor(const std::string& name) const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  auto it = monitors_.find(name);
  if (it != monitors_.end() && it->second.isMember("active_client")) {
    return it->second["active_client"];
  }
  return Json::nullValue;
}

std::string IPC::getLayoutSymbolForMonitor(const std::string& name) const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  auto it = monitors_.find(name);
  if (it != monitors_.end() && it->second.isMember("layout_symbol")) {
    return it->second["layout_symbol"].asString();
  }
  return {};
}

void IPC::handleMonitorUpdate(const Json::Value& mon) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  monitors_[mon["name"].asString()] = mon;
}

void IPC::updateFocusingClient(const Json::Value& client) {
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    active_client_ = client;

    if (client.isNull() || !client.isObject() || client["id"].isNull()) {
      focusing_client_id_ = 0;
    } else {
      focusing_client_id_ = client["id"].asUInt64();
      clients_[focusing_client_id_] = client;
    }
  }
}

void IPC::updateKeyboardLayout(const std::string& layout) {
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    keyboard_layout_ = layout;
  }
}

void IPC::registerForIPC(const std::string& ev, EventHandler* handler) {
  if (!handler) return;
  std::lock_guard<std::mutex> lock(callback_mutex_);
  callbacks_.emplace_back(ev, handler);
}

void IPC::unregisterForIPC(EventHandler* handler) {
  if (!handler) return;
  std::lock_guard<std::mutex> lock(callback_mutex_);
  for (auto it = callbacks_.begin(); it != callbacks_.end();) {
    if (it->second == handler)
      it = callbacks_.erase(it);
    else
      ++it;
  }
}

}  // namespace waybar::modules::mango