#include "modules/keyboard_state.hpp"
#include <filesystem>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

waybar::modules::KeyboardState::KeyboardState(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "keyboard_state", id, false, !config["disable-scroll"].asBool()),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
      numlock_label_(""),
      capslock_label_(""),
      numlock_format_(config_["format"].isString() ? config_["format"].asString()
                      : config_["format"]["numlock"].isString() ? config_["format"]["numlock"].asString()
                      : "{name} {icon}"),
      capslock_format_(config_["format"].isString() ? config_["format"].asString()
                       : config_["format"]["capslock"].isString() ? config_["format"]["capslock"].asString()
                       : "{name} {icon}"),
      scrolllock_format_(config_["format"].isString() ? config_["format"].asString()
                         : config_["format"]["scrolllock"].isString() ? config_["format"]["scrolllock"].asString()
                         : "{name} {icon}"),
      interval_(std::chrono::seconds(config_["interval"].isUInt() ? config_["interval"].asUInt() : 1)),
      icon_locked_(config_["format-icons"]["locked"].isString()
                   ? config_["format-icons"]["locked"].asString()
                   : "locked"),
      icon_unlocked_(config_["format-icons"]["unlocked"].isString()
                     ? config_["format-icons"]["unlocked"].asString()
                     : "unlocked")
 {
  box_.set_name("keyboard_state");
  if (config_["numlock"].asBool()) {
    box_.pack_end(numlock_label_, false, false, 0);
  }
  if (config_["capslock"].asBool()) {
    box_.pack_end(capslock_label_, false, false, 0);
  }
  if (config_["scrolllock"].asBool()) {
    box_.pack_end(scrolllock_label_, false, false, 0);
  }
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);

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
  if (!libevdev_has_event_code(dev_, EV_LED, LED_NUML)
      || !libevdev_has_event_code(dev_, EV_LED, LED_CAPSL)
      || !libevdev_has_event_code(dev_, EV_LED, LED_SCROLLL)) {
    throw std::runtime_error("Device doesn't support num lock, caps lock, or scroll lock events");
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
  int capsl = libevdev_get_event_value(dev_, EV_LED, LED_CAPSL);
  int scrolll = libevdev_get_event_value(dev_, EV_LED, LED_SCROLLL);

  std::string text;
  text = fmt::format(numlock_format_,
                     fmt::arg("icon", numl ? icon_locked_ : icon_unlocked_),
                     fmt::arg("name", "Num"));
  numlock_label_.set_markup(text);
  text = fmt::format(capslock_format_,
                     fmt::arg("icon", capsl ? icon_locked_ : icon_unlocked_),
                     fmt::arg("name", "Caps"));
  capslock_label_.set_markup(text);
  text = fmt::format(scrolllock_format_,
                     fmt::arg("icon", scrolll ? icon_locked_ : icon_unlocked_),
                     fmt::arg("name", "Scroll"));
  scrolllock_label_.set_markup(text);

  AModule::update();
}
