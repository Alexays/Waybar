#include "util/backlight_backend.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <sys/epoll.h>

#include <cmath>
#include <optional>
#include <utility>

namespace {
class FileDescriptor {
 public:
  explicit FileDescriptor(int fd) : fd_(fd) {}
  FileDescriptor(const FileDescriptor &other) = delete;
  FileDescriptor(FileDescriptor &&other) noexcept = delete;
  FileDescriptor &operator=(const FileDescriptor &other) = delete;
  FileDescriptor &operator=(FileDescriptor &&other) noexcept = delete;
  ~FileDescriptor() {
    if (fd_ != -1) {
      if (close(fd_) != 0) {
        fmt::print(stderr, "Failed to close fd: {}\n", errno);
      }
    }
  }
  int get() const { return fd_; }

 private:
  int fd_;
};

struct UdevDeleter {
  void operator()(udev *ptr) { udev_unref(ptr); }
};

struct UdevDeviceDeleter {
  void operator()(udev_device *ptr) { udev_device_unref(ptr); }
};

struct UdevEnumerateDeleter {
  void operator()(udev_enumerate *ptr) { udev_enumerate_unref(ptr); }
};

struct UdevMonitorDeleter {
  void operator()(udev_monitor *ptr) { udev_monitor_unref(ptr); }
};

void check_eq(int rc, int expected, const char *message = "eq, rc was: ") {
  if (rc != expected) {
    throw std::runtime_error(fmt::format(fmt::runtime(message), rc));
  }
}

void check_neq(int rc, int bad_rc, const char *message = "neq, rc was: ") {
  if (rc == bad_rc) {
    throw std::runtime_error(fmt::format(fmt::runtime(message), rc));
  }
}

void check0(int rc, const char *message = "rc wasn't 0") { check_eq(rc, 0, message); }

void check_gte(int rc, int gte, const char *message = "rc was: ") {
  if (rc < gte) {
    throw std::runtime_error(fmt::format(fmt::runtime(message), rc));
  }
}

void check_nn(const void *ptr, const char *message = "ptr was null") {
  if (ptr == nullptr) {
    throw std::runtime_error(message);
  }
}

}  // namespace

namespace waybar::util {

static void upsert_device(std::vector<BacklightDevice> &devices, udev_device *dev) {
  const char *name = udev_device_get_sysname(dev);
  check_nn(name);

  const char *actual_brightness_attr =
      strncmp(name, "amdgpu_bl", 9) == 0 || strcmp(name, "apple-panel-bl") == 0
          ? "brightness"
          : "actual_brightness";

  const char *actual = udev_device_get_sysattr_value(dev, actual_brightness_attr);
  const char *max = udev_device_get_sysattr_value(dev, "max_brightness");
  const char *power = udev_device_get_sysattr_value(dev, "bl_power");

  auto found = std::find_if(devices.begin(), devices.end(), [name](const BacklightDevice &device) {
    return device.name() == name;
  });
  if (found != devices.end()) {
    if (actual != nullptr) {
      found->set_actual(std::stoi(actual));
    }
    if (max != nullptr) {
      found->set_max(std::stoi(max));
    }
    if (power != nullptr) {
      found->set_powered(std::stoi(power) == 0);
    }
  } else {
    const int actual_int = actual == nullptr ? 0 : std::stoi(actual);
    const int max_int = max == nullptr ? 0 : std::stoi(max);
    const bool power_bool = power == nullptr ? true : std::stoi(power) == 0;
    devices.emplace_back(name, actual_int, max_int, power_bool);
  }
}

static void enumerate_devices(std::vector<BacklightDevice> &devices, udev *udev) {
  std::unique_ptr<udev_enumerate, UdevEnumerateDeleter> enumerate{udev_enumerate_new(udev)};
  udev_enumerate_add_match_subsystem(enumerate.get(), "backlight");
  udev_enumerate_scan_devices(enumerate.get());
  udev_list_entry *enum_devices = udev_enumerate_get_list_entry(enumerate.get());
  udev_list_entry *dev_list_entry;
  udev_list_entry_foreach(dev_list_entry, enum_devices) {
    const char *path = udev_list_entry_get_name(dev_list_entry);
    std::unique_ptr<udev_device, UdevDeviceDeleter> dev{udev_device_new_from_syspath(udev, path)};
    check_nn(dev.get(), "dev new failed");
    upsert_device(devices, dev.get());
  }
}

BacklightDevice::BacklightDevice(std::string name, int actual, int max, bool powered)
    : name_(std::move(name)), actual_(actual), max_(max), powered_(powered) {}

std::string BacklightDevice::name() const { return name_; }

int BacklightDevice::get_actual() const { return actual_; }

void BacklightDevice::set_actual(int actual) { actual_ = actual; }

int BacklightDevice::get_max() const { return max_; }

void BacklightDevice::set_max(int max) { max_ = max; }

bool BacklightDevice::get_powered() const { return powered_; }

void BacklightDevice::set_powered(bool powered) { powered_ = powered; }

BacklightBackend::BacklightBackend(std::chrono::milliseconds interval,
                                   std::function<void()> on_updated_cb)
    : on_updated_cb_(std::move(on_updated_cb)), polling_interval_(interval), previous_best_({}) {
  std::unique_ptr<udev, UdevDeleter> udev_check{udev_new()};
  check_nn(udev_check.get(), "Udev check new failed");
  enumerate_devices(devices_, udev_check.get());
  if (devices_.empty()) {
    throw std::runtime_error("No backlight found");
  }

  // Connect to the login interface
  login_proxy_ = Gio::DBus::Proxy::create_for_bus_sync(
      Gio::DBus::BusType::BUS_TYPE_SYSTEM, "org.freedesktop.login1",
      "/org/freedesktop/login1/session/self", "org.freedesktop.login1.Session");

  udev_thread_ = [this] {
    std::unique_ptr<udev, UdevDeleter> udev{udev_new()};
    check_nn(udev.get(), "Udev new failed");

    std::unique_ptr<udev_monitor, UdevMonitorDeleter> mon{
        udev_monitor_new_from_netlink(udev.get(), "udev")};
    check_nn(mon.get(), "udev monitor new failed");
    check_gte(udev_monitor_filter_add_match_subsystem_devtype(mon.get(), "backlight", nullptr), 0,
              "udev failed to add monitor filter: ");
    udev_monitor_enable_receiving(mon.get());

    auto udev_fd = udev_monitor_get_fd(mon.get());

    auto epoll_fd = FileDescriptor{epoll_create1(EPOLL_CLOEXEC)};
    check_neq(epoll_fd.get(), -1, "epoll init failed: ");
    epoll_event ctl_event{};
    ctl_event.events = EPOLLIN;
    ctl_event.data.fd = udev_fd;

    check0(epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, ctl_event.data.fd, &ctl_event),
           "epoll_ctl failed: {}");
    epoll_event events[EPOLL_MAX_EVENTS];

    while (udev_thread_.isRunning()) {
      const int event_count =
          epoll_wait(epoll_fd.get(), events, EPOLL_MAX_EVENTS, this->polling_interval_.count());
      if (!udev_thread_.isRunning()) {
        break;
      }
      decltype(devices_) devices;
      {
        std::scoped_lock<std::mutex> lock(udev_thread_mutex_);
        devices = devices_;
      }
      for (int i = 0; i < event_count; ++i) {
        const auto &event = events[i];
        check_eq(event.data.fd, udev_fd, "unexpected udev fd");
        std::unique_ptr<udev_device, UdevDeviceDeleter> dev{udev_monitor_receive_device(mon.get())};
        check_nn(dev.get(), "epoll dev was null");
        upsert_device(devices, dev.get());
      }

      // Refresh state if timed out
      if (event_count == 0) {
        enumerate_devices(devices, udev.get());
      }
      {
        std::scoped_lock<std::mutex> lock(udev_thread_mutex_);
        devices_ = devices;
      }
      this->on_updated_cb_();
    }
  };
}

const BacklightDevice *BacklightBackend::best_device(const std::vector<BacklightDevice> &devices,
                                                     std::string_view preferred_device) {
  const auto found = std::find_if(
      devices.begin(), devices.end(),
      [preferred_device](const BacklightDevice &dev) { return dev.name() == preferred_device; });
  if (found != devices.end()) {
    return &(*found);
  }

  const auto max = std::max_element(
      devices.begin(), devices.end(),
      [](const BacklightDevice &l, const BacklightDevice &r) { return l.get_max() < r.get_max(); });

  return max == devices.end() ? nullptr : &(*max);
}

const BacklightDevice *BacklightBackend::get_previous_best_device() {
  return previous_best_.has_value() ? &(*previous_best_) : nullptr;
}

void BacklightBackend::set_previous_best_device(const BacklightDevice *device) {
  if (device == nullptr) {
    previous_best_ = std::nullopt;
  } else {
    previous_best_ = std::optional{*device};
  }
}

void BacklightBackend::set_scaled_brightness(const std::string &preferred_device, int brightness) {
  GET_BEST_DEVICE(best, (*this), preferred_device);

  if (best != nullptr) {
    const auto max = best->get_max();
    const auto abs_val = static_cast<int>(std::round(brightness * max / 100.0F));
    set_brightness_internal(best->name(), abs_val, best->get_max());
  }
}

void BacklightBackend::set_brightness(const std::string &preferred_device, ChangeType change_type,
                                      double step) {
  GET_BEST_DEVICE(best, (*this), preferred_device);

  if (best != nullptr) {
    const auto max = best->get_max();

    const auto abs_step = static_cast<int>(round(step * max / 100.0F));

    const int new_brightness = change_type == ChangeType::Increase ? best->get_actual() + abs_step
                                                                   : best->get_actual() - abs_step;
    set_brightness_internal(best->name(), new_brightness, max);
  }
}

void BacklightBackend::set_brightness_internal(const std::string &device_name, int brightness,
                                               int max_brightness) {
  brightness = std::clamp(brightness, 0, max_brightness);

  auto call_args = Glib::VariantContainerBase(
      g_variant_new("(ssu)", "backlight", device_name.c_str(), brightness));

  login_proxy_->call_sync("SetBrightness", call_args);
}

int BacklightBackend::get_scaled_brightness(const std::string &preferred_device) {
  GET_BEST_DEVICE(best, (*this), preferred_device);

  if (best != nullptr) {
    return best->get_actual() * 100 / best->get_max();
  }

  return 0;
}

}  // namespace waybar::util
