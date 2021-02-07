#include "modules/keyboard_state.hpp"
#include <filesystem>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

waybar::modules::KeyboardState::KeyboardState(const std::string& id, const Json::Value& config)
    : ALabel(config, "keyboard_state", id, "{temperatureC}", 1) {
  if (config_["device-path"].isString()) {
    dev_path_ = config_["device-path"].asString();
  } else {
    dev_path_ = "";
  }

  fd_ = open(dev_path_.c_str(), O_NONBLOCK | O_CLOEXEC | O_RDONLY);
  if (fd_ < 0) {
    throw std::runtime_error("Can't open " + dev_path_);
  }
  int err = libevdev_new_from_fd(fd_, &dev_);
  if (err < 0) {
    throw std::runtime_error("Can't create libevdev device");
  }
  if (!libevdev_has_event_type(dev_, EV_LED)) {
    throw std::runtime_error("Device doesn't support LED events");
  }
  if (!libevdev_has_event_code(dev_, EV_LED, LED_NUML) || !libevdev_has_event_code(dev_, EV_LED, LED_CAPSL)) {
    throw std::runtime_error("Device doesn't support num lock or caps lock events");
  }

  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

waybar::modules::KeyboardState::~KeyboardState() {
  libevdev_free(dev_);
  int err = close(fd_);
  if (err < 0) {
    // Not much we can do, so ignore it.
  }
}

auto waybar::modules::KeyboardState::update() -> void {
  int err = LIBEVDEV_READ_STATUS_SUCCESS;
  while (err == LIBEVDEV_READ_STATUS_SUCCESS) {
    input_event ev;
    err = libevdev_next_event(dev_, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    while (err == LIBEVDEV_READ_STATUS_SYNC) {
      err = libevdev_next_event(dev_, LIBEVDEV_READ_FLAG_SYNC, &ev);
    }
  }
  if (err != -EAGAIN) {
    throw std::runtime_error("Failed to sync evdev device");
  }

  int numl = libevdev_get_event_value(dev_, EV_LED, LED_NUML);
  //int capsl = libevdev_get_event_value(dev_, EV_LED, LED_CAPSL);

  std::string text;
  if (numl) {
    text = fmt::format(format_, "num lock enabled");
    label_.set_markup(text);
  } else {
    text = fmt::format(format_, "num lock disabled");
    label_.set_markup(text);
  }

  ALabel::update();
}
