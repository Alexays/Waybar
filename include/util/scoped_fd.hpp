#pragma once

#include <unistd.h>

namespace waybar::util {

class ScopedFd {
 public:
  explicit ScopedFd(int fd = -1) : fd_(fd) {}
  ~ScopedFd() {
    if (fd_ != -1) {
      close(fd_);
    }
  }

  // ScopedFd is non-copyable
  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;

  // ScopedFd is moveable
  ScopedFd(ScopedFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
  ScopedFd& operator=(ScopedFd&& other) noexcept {
    if (this != &other) {
      if (fd_ != -1) {
        close(fd_);
      }
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  int get() const { return fd_; }

  operator int() const { return fd_; }

  void reset(int fd = -1) {
    if (fd_ != -1) {
      close(fd_);
    }
    fd_ = fd;
  }

  int release() {
    int fd = fd_;
    fd_ = -1;
    return fd;
  }

 private:
  int fd_;
};

}  // namespace waybar::util
