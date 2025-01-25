#pragma once

#include <libudev.h>

namespace waybar::util {
struct UdevDeleter {
  void operator()(udev *ptr) const { udev_unref(ptr); }
};

struct UdevDeviceDeleter {
  void operator()(udev_device *ptr) const { udev_device_unref(ptr); }
};

struct UdevEnumerateDeleter {
  void operator()(udev_enumerate *ptr) const { udev_enumerate_unref(ptr); }
};

struct UdevMonitorDeleter {
  void operator()(udev_monitor *ptr) const { udev_monitor_unref(ptr); }
};
}  // namespace waybar::util