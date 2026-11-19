#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <libudev.h>
#include <umockdev.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

#include "util/backlight_backend.hpp"

namespace {

// Observe real libudev calls without replacing discovery, sysfs reads, or events.
std::atomic<const char*> selected_name = nullptr;
std::atomic<unsigned> unrelated_reads = 0;
std::atomic<unsigned> unrelated_events = 0;

class BacklightTestbed {
 public:
  BacklightTestbed() : testbed_(umockdev_testbed_new(), g_object_unref) {
    REQUIRE(umockdev_in_mock_environment());
    selected_name = nullptr;
    unrelated_reads = 0;
    unrelated_events = 0;
    addDevice("backlight", "primary_bl", "40", "100");
    addDevice("backlight", "secondary_bl", "200", "1000");
    addDevice("leds", "white:kbd_backlight", "700", "10000");
  }

  void addDevice(const char* subsystem, const char* name, const char* actual, const char* max) {
    std::unique_ptr<gchar, decltype(&g_free)> path(
        umockdev_testbed_add_device(testbed_.get(), subsystem, name, nullptr, "actual_brightness",
                                    actual, "max_brightness", max, "bl_power", "0", nullptr,
                                    nullptr),
        g_free);
    REQUIRE(path != nullptr);
  }

  void setBrightness(const char* name, const char* actual) {
    const auto path = std::string("/sys/devices/") + name;
    umockdev_testbed_set_attribute(testbed_.get(), path.c_str(), "actual_brightness", actual);
  }

  void changeDevice(const char* name) {
    const auto path = std::string("/sys/devices/") + name;
    umockdev_testbed_uevent(testbed_.get(), path.c_str(), "change");
  }

  void onUpdate() {
    std::lock_guard lock(mutex_);
    ++updates_;
    processed_unrelated_events_ = unrelated_events.load();
    condition_.notify_all();
  }

  template <typename Predicate>
  bool waitFor(Predicate predicate) {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(3), predicate);
  }

  bool waitForMonitor() {
    return waitFor([this] { return updates_ > 0; });
  }

  bool waitForUnrelatedEvents() {
    return waitFor([this] { return processed_unrelated_events_ >= 2; });
  }

  bool waitForMoreUpdates(unsigned count) {
    std::unique_lock lock(mutex_);
    const unsigned target = updates_ + count;
    return condition_.wait_for(lock, std::chrono::seconds(3),
                               [this, target] { return updates_ >= target; });
  }

 private:
  std::unique_ptr<UMockdevTestbed, decltype(&g_object_unref)> testbed_;
  std::mutex mutex_;
  std::condition_variable condition_;
  unsigned updates_ = 0;
  unsigned processed_unrelated_events_ = 0;
};

}  // namespace

extern "C" {
const char* __real_udev_device_get_sysattr_value(udev_device* device, const char* attribute);
udev_device* __real_udev_monitor_receive_device(udev_monitor* monitor);

const char* __wrap_udev_device_get_sysattr_value(udev_device* device, const char* attribute) {
  const char* selected = selected_name;
  const char* name = udev_device_get_sysname(device);
  if (selected != nullptr && name != nullptr && std::strcmp(name, selected) != 0) {
    ++unrelated_reads;
  }
  return __real_udev_device_get_sysattr_value(device, attribute);
}

udev_device* __wrap_udev_monitor_receive_device(udev_monitor* monitor) {
  auto* device = __real_udev_monitor_receive_device(monitor);
  const char* selected = selected_name;
  if (device != nullptr && selected != nullptr) {
    const char* name = udev_device_get_sysname(device);
    if (name != nullptr && std::strcmp(name, selected) != 0) {
      ++unrelated_events;
    }
  }
  return device;
}
}

namespace waybar::util {
SafeSignal<bool>& prepare_for_sleep() {
  static SafeSignal<bool> signal;
  return signal;
}
}  // namespace waybar::util

TEST_CASE("Explicit backlight devices do not read unrelated hardware", "[util][backlight]") {
  BacklightTestbed testbed;
  const char* selected = nullptr;
  int initial_brightness = 0;
  const char* updated_brightness = nullptr;
  SECTION("Screen backlight") {
    selected = "primary_bl";
    initial_brightness = 40;
    updated_brightness = "65";
  }
  SECTION("Keyboard backlight") {
    selected = "white:kbd_backlight";
    initial_brightness = 7;
    updated_brightness = "6500";
  }
  selected_name = selected;
  waybar::util::BacklightBackend backend(
      std::chrono::milliseconds(20), [&testbed] { testbed.onUpdate(); }, selected);
  REQUIRE(backend.get_scaled_brightness(selected) == initial_brightness);
  REQUIRE(unrelated_reads == 0);
  REQUIRE(testbed.waitForMonitor());

  testbed.setBrightness(selected, updated_brightness);
  REQUIRE(testbed.waitFor([&] { return backend.get_scaled_brightness(selected) == 65; }));
  REQUIRE(unrelated_reads == 0);

  // Both change events and newly discovered devices must be rejected before reading attributes.
  testbed.changeDevice("secondary_bl");
  testbed.addDevice("backlight", "hotplug_bl", "1500", "2000");
  REQUIRE(testbed.waitForUnrelatedEvents());
  REQUIRE(unrelated_reads == 0);
  REQUIRE(backend.get_scaled_brightness(selected) == 65);
}

TEST_CASE("Automatic backlight selection still discovers and refreshes devices",
          "[util][backlight]") {
  BacklightTestbed testbed;
  waybar::util::BacklightBackend backend(
      std::chrono::milliseconds(20), [&testbed] { testbed.onUpdate(); }, "");
  // A screen backlight outranks the keyboard LED, even with its larger maximum.
  REQUIRE(backend.get_scaled_brightness("") == 20);
  REQUIRE(testbed.waitForMonitor());
  testbed.setBrightness("secondary_bl", "800");
  REQUIRE(testbed.waitFor([&] { return backend.get_scaled_brightness("") == 80; }));

  testbed.addDevice("backlight", "hotplug_bl", "1500", "2000");
  REQUIRE(testbed.waitFor([&] { return backend.get_scaled_brightness("") == 75; }));
}

TEST_CASE("Missing explicit backlights fall back, then are adopted exclusively when they appear",
          "[util][backlight]") {
  BacklightTestbed testbed;
  waybar::util::BacklightBackend backend(
      std::chrono::milliseconds(20), [&testbed] { testbed.onUpdate(); }, "late_bl");
  // Not present at startup: automatic selection (largest screen backlight) is used.
  REQUIRE(backend.get_scaled_brightness("late_bl") == 20);
  REQUIRE(testbed.waitForMonitor());

  testbed.addDevice("backlight", "late_bl", "900", "1000");
  REQUIRE(testbed.waitFor([&] { return backend.get_scaled_brightness("late_bl") == 90; }));

  // From now on only the configured device may be tracked, refreshed or read.
  selected_name = "late_bl";
  unrelated_reads = 0;
  unrelated_events = 0;
  testbed.changeDevice("secondary_bl");
  testbed.changeDevice("primary_bl");
  REQUIRE(testbed.waitForUnrelatedEvents());
  REQUIRE(testbed.waitForMoreUpdates(5));
  REQUIRE(unrelated_reads == 0);
  REQUIRE(backend.get_scaled_brightness("late_bl") == 90);
}
