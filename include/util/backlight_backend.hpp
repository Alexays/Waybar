#pragma once

#include <libudev.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "giomm/dbusproxy.h"
#include "util/backend_common.hpp"
#include "util/sleeper_thread.hpp"

#define GET_BEST_DEVICE(varname, backend, preferred_device)          \
  decltype((backend).devices_) __devices;                            \
  {                                                                  \
    std::scoped_lock<std::mutex> lock((backend).udev_thread_mutex_); \
    __devices = (backend).devices_;                                  \
  }                                                                  \
  auto varname = (backend).best_device(__devices, preferred_device);

namespace waybar::util {

class BacklightDevice {
 public:
  BacklightDevice() = default;
  BacklightDevice(std::string name, int actual, int max, bool powered);

  std::string name() const;
  int get_actual() const;
  void set_actual(int actual);
  int get_max() const;
  void set_max(int max);
  bool get_powered() const;
  void set_powered(bool powered);
  friend inline bool operator==(const BacklightDevice &lhs, const BacklightDevice &rhs) {
    return lhs.name_ == rhs.name_ && lhs.actual_ == rhs.actual_ && lhs.max_ == rhs.max_;
  }

 private:
  std::string name_;
  int actual_ = 1;
  int max_ = 1;
  bool powered_ = true;
};

class BacklightBackend {
 public:
  BacklightBackend(std::chrono::milliseconds interval, std::function<void()> on_updated_cb = NOOP);

  // const inline BacklightDevice *get_best_device(std::string_view preferred_device);
  const BacklightDevice *get_previous_best_device();

  void set_previous_best_device(const BacklightDevice *device);

  void set_brightness(const std::string &preferred_device, ChangeType change_type, double step);

  void set_scaled_brightness(const std::string &preferred_device, int brightness);
  int get_scaled_brightness(const std::string &preferred_device);

  bool is_login_proxy_initialized() const { return static_cast<bool>(login_proxy_); }

  static const BacklightDevice *best_device(const std::vector<BacklightDevice> &devices,
                                            std::string_view);

  std::vector<BacklightDevice> devices_;
  std::mutex udev_thread_mutex_;

 private:
  void set_brightness_internal(const std::string &device_name, int brightness, int max_brightness);

  std::function<void()> on_updated_cb_;
  std::chrono::milliseconds polling_interval_;

  std::optional<BacklightDevice> previous_best_;
  // thread must destruct before shared data
  util::SleeperThread udev_thread_;

  Glib::RefPtr<Gio::DBus::Proxy> login_proxy_;

  static constexpr int EPOLL_MAX_EVENTS = 16;
};

}  // namespace waybar::util
