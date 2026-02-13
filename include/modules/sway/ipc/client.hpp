#pragma once

#include <sigc++/sigc++.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include "ipc.hpp"
#include "util/sleeper_thread.hpp"
#include "util/SafeSignal.hpp"

namespace waybar::modules::sway {

class Ipc {
 public:
  Ipc();
  ~Ipc();

  struct ipc_response {
    uint32_t size;
    uint32_t type;
    std::string payload;
  };

  ::waybar::SafeSignal<const struct ipc_response&> signal_event;
  ::waybar::SafeSignal<const struct ipc_response&> signal_cmd;

  void sendCmd(uint32_t type, const std::string& payload = "");
  void subscribe(const std::string& payload);
  void handleEvent();
  void setWorker(std::function<void()>&& func);

 protected:
  static inline const std::string ipc_magic_ = "i3-ipc";
  static inline const size_t ipc_header_size_ = ipc_magic_.size() + 8;

  const std::string getSocketPath() const;
  int open(const std::string&) const;
  struct ipc_response send(int fd, uint32_t type, const std::string& payload = "");
  struct ipc_response recv(int fd);

  int fd_;
  int fd_event_;
  std::mutex mutex_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules::sway
