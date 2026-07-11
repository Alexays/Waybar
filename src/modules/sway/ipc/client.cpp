#include "modules/sway/ipc/client.hpp"

#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#include "modules/sway/ipc/ipc.hpp"

namespace waybar::modules::sway {
namespace {

void sendAll(int fd, const char* data, size_t size, const char* what) {
  size_t total = 0;
  while (total < size) {
    const auto res = ::send(fd, data + total, size - total, 0);
    if (res < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        continue;
      }
      throw std::runtime_error(what);
    }
    if (res == 0) {
      throw std::runtime_error(what);
    }
    total += static_cast<size_t>(res);
  }
}

}  // namespace

Ipc::Ipc() {
  socketPath_ = getSocketPath();
  fd_ = util::ScopedFd(open(socketPath_));
  fd_event_ = util::ScopedFd(open(socketPath_));
}

Ipc::~Ipc() {
  // Signal the worker before stopping it so an in-flight recv/reconnect bails
  // out instead of trying to reconnect to a socket we're tearing down.
  running_ = false;
  thread_.stop();

  if (fd_ > 0) {
    // To fail the IPC header
    if (write(fd_, "close-sway-ipc", 14) == -1) {
      spdlog::error("Failed to close sway IPC");
    }
  }
  if (fd_event_ > 0) {
    if (write(fd_event_, "close-sway-ipc", 14) == -1) {
      spdlog::error("Failed to close sway IPC event handler");
    }
  }
}

void Ipc::setWorker(std::function<void()>&& func) { thread_ = std::move(func); }

std::string Ipc::getSocketPath() {
  const char* env = getenv("SWAYSOCK");
  if (env != nullptr && env[0] != '\0') {
    return {env};
  }

  FILE* in = popen("sway --get-socketpath 2>/dev/null", "r");
  if (in == nullptr) {
    throw std::runtime_error("Failed to get socket path");
  }

  std::string str;
  char buf[512] = {0};
  while (fgets(buf, sizeof(buf), in) != nullptr) {
    str.append(buf);
  }

  if (pclose(in) == -1) {
    throw std::runtime_error("Failed to get socket path");
  }

  if (str.ends_with('\n')) {
    str.pop_back();
  }
  if (str.empty()) {
    throw std::runtime_error("Socket path is empty");
  }

  return str;
}

int Ipc::open(const std::string& socketPath) {
  util::ScopedFd fd(socket(AF_UNIX, SOCK_STREAM, 0));
  if (fd == -1) {
    throw std::runtime_error("Unable to open Unix socket");
  }
  (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
  struct sockaddr_un addr{.sun_family = AF_UNIX};
  strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
  if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof addr) == -1) {
    throw std::runtime_error("Unable to connect to Sway");
  }
  return fd.release();
}

struct Ipc::ipc_response Ipc::recv(int fd) {
  std::string header;
  header.resize(ipc_header_size_);

  size_t total = 0;
  while (total < ipc_header_size_) {
    const ssize_t res = ::recv(fd, header.data() + total, ipc_header_size_ - total, 0);
    if (fd_event_ == -1 || fd_ == -1) {
      // IPC is closed so just return an empty response
      return {.size = 0, .type = 0, .payload = ""};
    }
    if (res < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        continue;
      }
      throw std::runtime_error("Unable to receive IPC header");
    }
    if (res == 0) {
      throw std::runtime_error("Unable to receive IPC header");
    }
    total += static_cast<size_t>(res);
  }
  if (std::string_view(header.data(), ipc_magic_.size()) != ipc_magic_) {
    throw std::runtime_error("Invalid IPC magic");
  }

  uint32_t payload_size = 0;
  uint32_t payload_type = 0;
  memcpy(&payload_size, header.data() + ipc_magic_.size(), sizeof payload_size);
  memcpy(&payload_type, header.data() + ipc_magic_.size() + sizeof payload_size,
         sizeof payload_type);

  std::string payload;
  payload.resize(payload_size);

  total = 0;
  while (total < payload_size) {
    const ssize_t res = ::recv(fd, payload.data() + total, payload_size - total, 0);
    if (res < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        continue;
      }
      throw std::runtime_error("Unable to receive IPC payload");
    }
    if (res == 0) {
      throw std::runtime_error("Unable to receive IPC payload");
    }
    total += static_cast<size_t>(res);
  }

  return {.size = payload_size, .type = payload_type, .payload = std::move(payload)};
}

struct Ipc::ipc_response Ipc::send(int fd, uint32_t type, const std::string& payload) {
  std::string header;
  header.resize(ipc_header_size_);
  memcpy(header.data(), ipc_magic_.data(), ipc_magic_.size());
  if (payload.size() > std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error("IPC payload is too large");
  }
  const auto payload_size = static_cast<uint32_t>(payload.size());
  memcpy(header.data() + ipc_magic_.size(), &payload_size, sizeof payload_size);
  memcpy(header.data() + ipc_magic_.size() + sizeof payload_size, &type, sizeof type);

  sendAll(fd, header.data(), ipc_header_size_, "Unable to send IPC header");
  sendAll(fd, payload.data(), payload.size(), "Unable to send IPC payload");

  return Ipc::recv(fd);
}

void Ipc::sendCmd(uint32_t type, const std::string& payload) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto res = Ipc::send(fd_, type, payload);
  signal_cmd.emit(res);
}

void Ipc::subscribe(const std::string& payload) {
  auto res = Ipc::send(fd_event_, IPC_SUBSCRIBE, payload);
  if (res.payload != "{\"success\": true}") {
    throw std::runtime_error("Unable to subscribe ipc event");
  }
  // Remember the subscription so we can replay it if we have to reconnect.
  subscribed_events_.push_back(payload);
}

void Ipc::reconnectEvent() {
  // Sway closed our event connection (typically because its send buffer filled
  // up during an event flood). Re-establish the socket and re-subscribe to the
  // same events, backing off between attempts so we don't busy-loop and peg a
  // CPU while sway is unavailable or keeps dropping us.
  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (!running_) {
      return;
    }
    try {
      fd_event_.reset(open(socketPath_));
      for (const auto& payload : subscribed_events_) {
        const auto res = Ipc::send(fd_event_, IPC_SUBSCRIBE, payload);
        if (res.payload != "{\"success\": true}") {
          throw std::runtime_error("Unable to re-subscribe ipc event");
        }
      }
      spdlog::info("Reconnected to sway IPC event socket");
      return;
    } catch (const std::exception& e) {
      spdlog::warn("Failed to reconnect to sway IPC ({}), retrying", e.what());
    }
  }
}

void Ipc::handleEvent() {
  try {
    const auto res = Ipc::recv(fd_event_);
    signal_event.emit(res);
  } catch (const std::exception& e) {
    if (!running_) {
      // The Ipc is being torn down; the socket was closed on purpose.
      return;
    }
    spdlog::warn("Lost sway IPC event connection ({}), reconnecting", e.what());
    reconnectEvent();
  }
}

}  // namespace waybar::modules::sway
