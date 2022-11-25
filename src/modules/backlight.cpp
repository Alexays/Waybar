#include "modules/backlight.hpp"

#include <fmt/format.h>
#include <libudev.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <memory>

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
    throw std::runtime_error(fmt::format(message, rc));
  }
}

void check_neq(int rc, int bad_rc, const char *message = "neq, rc was: ") {
  if (rc == bad_rc) {
    throw std::runtime_error(fmt::format(message, rc));
  }
}

void check0(int rc, const char *message = "rc wasn't 0") { check_eq(rc, 0, message); }

void check_gte(int rc, int gte, const char *message = "rc was: ") {
  if (rc < gte) {
    throw std::runtime_error(fmt::format(message, rc));
  }
}

void check_nn(const void *ptr, const char *message = "ptr was null") {
  if (ptr == nullptr) {
    throw std::runtime_error(message);
  }
}
}  // namespace

waybar::modules::Backlight::BacklightDev::BacklightDev(std::string name, int actual, int max,
                                                       bool powered)
    : name_(std::move(name)), actual_(actual), max_(max), powered_(powered) {}

std::string_view waybar::modules::Backlight::BacklightDev::name() const { return name_; }

int waybar::modules::Backlight::BacklightDev::get_actual() const { return actual_; }

void waybar::modules::Backlight::BacklightDev::set_actual(int actual) { actual_ = actual; }

int waybar::modules::Backlight::BacklightDev::get_max() const { return max_; }

void waybar::modules::Backlight::BacklightDev::set_max(int max) { max_ = max; }

bool waybar::modules::Backlight::BacklightDev::get_powered() const { return powered_; }

void waybar::modules::Backlight::BacklightDev::set_powered(bool powered) { powered_ = powered; }

waybar::modules::Backlight::Backlight(const std::string &id, const Json::Value &config)
    : ALabel(config, "backlight", id, "{percent}%", 2),
      preferred_device_(config["device"].isString() ? config["device"].asString() : "") {
  // Get initial state
  {
    std::unique_ptr<udev, UdevDeleter> udev_check{udev_new()};
    check_nn(udev_check.get(), "Udev check new failed");
    enumerate_devices(devices_.begin(), devices_.end(), std::back_inserter(devices_),
                      udev_check.get());
    if (devices_.empty()) {
      throw std::runtime_error("No backlight found");
    }
    dp.emit();
  }

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
      const int event_count = epoll_wait(epoll_fd.get(), events, EPOLL_MAX_EVENTS,
                                         std::chrono::milliseconds{interval_}.count());
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
        upsert_device(devices.begin(), devices.end(), std::back_inserter(devices), dev.get());
      }

      // Refresh state if timed out
      if (event_count == 0) {
        enumerate_devices(devices.begin(), devices.end(), std::back_inserter(devices), udev.get());
      }
      {
        std::scoped_lock<std::mutex> lock(udev_thread_mutex_);
        devices_ = devices;
      }
      dp.emit();
    }
  };
}

waybar::modules::Backlight::~Backlight() = default;

auto waybar::modules::Backlight::update() -> void {
  decltype(devices_) devices;
  {
    std::scoped_lock<std::mutex> lock(udev_thread_mutex_);
    devices = devices_;
  }

  const auto best = best_device(devices.cbegin(), devices.cend(), preferred_device_);
  if (best != nullptr) {
    if (previous_best_.has_value() && previous_best_.value() == *best &&
        !previous_format_.empty() && previous_format_ == format_) {
      return;
    }

    if (best->get_powered()) {
      event_box_.show();
      const uint8_t percent =
          best->get_max() == 0 ? 100 : round(best->get_actual() * 100.0f / best->get_max());
      label_.set_markup(fmt::format(format_, fmt::arg("percent", std::to_string(percent)),
                                    fmt::arg("icon", getIcon(percent))));
      getState(percent);
    } else {
      event_box_.hide();
    }
  } else {
    if (!previous_best_.has_value()) {
      return;
    }
    label_.set_markup("");
  }
  previous_best_ = best == nullptr ? std::nullopt : std::optional{*best};
  previous_format_ = format_;
  // Call parent update
  ALabel::update();
}

template <class ForwardIt>
const waybar::modules::Backlight::BacklightDev *waybar::modules::Backlight::best_device(
    ForwardIt first, ForwardIt last, std::string_view preferred_device) {
  const auto found = std::find_if(
      first, last, [preferred_device](const auto &dev) { return dev.name() == preferred_device; });
  if (found != last) {
    return &(*found);
  }

  const auto max = std::max_element(
      first, last, [](const auto &l, const auto &r) { return l.get_max() < r.get_max(); });

  return max == last ? nullptr : &(*max);
}

template <class ForwardIt, class Inserter>
void waybar::modules::Backlight::upsert_device(ForwardIt first, ForwardIt last, Inserter inserter,
                                               udev_device *dev) {
  const char *name = udev_device_get_sysname(dev);
  check_nn(name);

  const char *actual_brightness_attr =
      strncmp(name, "amdgpu_bl", 9) == 0 ? "brightness" : "actual_brightness";

  const char *actual = udev_device_get_sysattr_value(dev, actual_brightness_attr);
  const char *max = udev_device_get_sysattr_value(dev, "max_brightness");
  const char *power = udev_device_get_sysattr_value(dev, "bl_power");

  auto found =
      std::find_if(first, last, [name](const auto &device) { return device.name() == name; });
  if (found != last) {
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
    *inserter = BacklightDev{name, actual_int, max_int, power_bool};
    ++inserter;
  }
}

template <class ForwardIt, class Inserter>
void waybar::modules::Backlight::enumerate_devices(ForwardIt first, ForwardIt last,
                                                   Inserter inserter, udev *udev) {
  std::unique_ptr<udev_enumerate, UdevEnumerateDeleter> enumerate{udev_enumerate_new(udev)};
  udev_enumerate_add_match_subsystem(enumerate.get(), "backlight");
  udev_enumerate_scan_devices(enumerate.get());
  udev_list_entry *enum_devices = udev_enumerate_get_list_entry(enumerate.get());
  udev_list_entry *dev_list_entry;
  udev_list_entry_foreach(dev_list_entry, enum_devices) {
    const char *path = udev_list_entry_get_name(dev_list_entry);
    std::unique_ptr<udev_device, UdevDeviceDeleter> dev{udev_device_new_from_syspath(udev, path)};
    check_nn(dev.get(), "dev new failed");
    upsert_device(first, last, inserter, dev.get());
  }
}
