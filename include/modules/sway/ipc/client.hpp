#pragma once

#include <sigc++/sigc++.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

#include "ipc.hpp"
#include "util/SafeSignal.hpp"
#include "util/scoped_fd.hpp"
#include "util/sleeper_thread.hpp"

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

  static std::string getSocketPath();
  static int open(const std::string&);

  struct ipc_response send(int fd, uint32_t type, const std::string& payload = "");
  struct ipc_response recv(int fd);

  util::ScopedFd fd_;
  util::ScopedFd fd_event_;
  std::mutex mutex_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules::sway
