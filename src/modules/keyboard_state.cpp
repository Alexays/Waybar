#include "modules/keyboard_state.hpp"

#include <errno.h>
#include <spdlog/spdlog.h>
#include <string.h>

#include <filesystem>

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

class errno_error : public std::runtime_error {
 public:
  int code;
  errno_error(int code, const std::string& msg)
      : std::runtime_error(getErrorMsg(code, msg.c_str())), code(code) {}
  errno_error(int code, const char* msg) : std::runtime_error(getErrorMsg(code, msg)), code(code) {}

 private:
  static auto getErrorMsg(int err, const char* msg) -> std::string {
    std::string error_msg{msg};
    error_msg += ": ";

#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 32)
    // strerrorname_np gets the error code's name; it's nice to have, but it's a recent GNU
    // extension
    const auto errno_name = strerrorname_np(err);
    error_msg += errno_name;
    error_msg += " ";
#endif

    const auto errno_str = strerror(err);
    error_msg += errno_str;

    return error_msg;
  }
};

auto openFile(const std::string& path, int flags) -> int {
  int fd = open(path.c_str(), flags);
  if (fd < 0) {
    if (errno == EACCES) {
      throw errno_error(errno, "Can't open " + path + " (are you in the input group?)");
    } else {
      throw errno_error(errno, "Can't open " + path);
    }
  }
  return fd;
}

auto closeFile(int fd) -> void {
  int res = close(fd);
  if (res < 0) {
    throw errno_error(errno, "Can't close file");
  }
}

auto openDevice(int fd) -> libevdev* {
  libevdev* dev;
  int err = libevdev_new_from_fd(fd, &dev);
  if (err < 0) {
    throw errno_error(-err, "Can't create libevdev device");
  }
  return dev;
}

auto supportsLockStates(const libevdev* dev) -> bool {
  return libevdev_has_event_type(dev, EV_LED) && libevdev_has_event_code(dev, EV_LED, LED_NUML) &&
         libevdev_has_event_code(dev, EV_LED, LED_CAPSL) &&
         libevdev_has_event_code(dev, EV_LED, LED_SCROLLL);
}

waybar::modules::KeyboardState::KeyboardState(const std::string& id, const Bar& bar,
                                              const Json::Value& config)
    : AModule(config, "keyboard-state", id, false, !config["disable-scroll"].asBool()),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
      numlock_label_(""),
      capslock_label_(""),
      numlock_format_(config_["format"].isString() ? config_["format"].asString()
                      : config_["format"]["numlock"].isString()
                          ? config_["format"]["numlock"].asString()
                          : "{name} {icon}"),
      capslock_format_(config_["format"].isString() ? config_["format"].asString()
                       : config_["format"]["capslock"].isString()
                           ? config_["format"]["capslock"].asString()
                           : "{name} {icon}"),
      scrolllock_format_(config_["format"].isString() ? config_["format"].asString()
                         : config_["format"]["scrolllock"].isString()
                             ? config_["format"]["scrolllock"].asString()
                             : "{name} {icon}"),
      interval_(
          std::chrono::seconds(config_["interval"].isUInt() ? config_["interval"].asUInt() : 1)),
      icon_locked_(config_["format-icons"]["locked"].isString()
                       ? config_["format-icons"]["locked"].asString()
                       : "locked"),
      icon_unlocked_(config_["format-icons"]["unlocked"].isString()
                         ? config_["format-icons"]["unlocked"].asString()
                         : "unlocked"),
      fd_(0),
      dev_(nullptr) {
  box_.set_name("keyboard-state");
  if (config_["numlock"].asBool()) {
    numlock_label_.get_style_context()->add_class("numlock");
    box_.pack_end(numlock_label_, false, false, 0);
  }
  if (config_["capslock"].asBool()) {
    capslock_label_.get_style_context()->add_class("capslock");
    box_.pack_end(capslock_label_, false, false, 0);
  }
  if (config_["scrolllock"].asBool()) {
    scrolllock_label_.get_style_context()->add_class("scrolllock");
    box_.pack_end(scrolllock_label_, false, false, 0);
  }
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);

  if (config_["device-path"].isString()) {
    std::string dev_path = config_["device-path"].asString();
    fd_ = openFile(dev_path, O_NONBLOCK | O_CLOEXEC | O_RDONLY);
    dev_ = openDevice(fd_);
  } else {
    DIR* dev_dir = opendir("/dev/input");
    if (dev_dir == nullptr) {
      throw errno_error(errno, "Failed to open /dev/input");
    }
    dirent* ep;
    while ((ep = readdir(dev_dir))) {
      if (ep->d_type != DT_CHR) continue;
      std::string dev_path = std::string("/dev/input/") + ep->d_name;
      int fd = openFile(dev_path.c_str(), O_NONBLOCK | O_CLOEXEC | O_RDONLY);
      try {
        auto dev = openDevice(fd);
        if (supportsLockStates(dev)) {
          spdlog::info("Found device {} at '{}'", libevdev_get_name(dev), dev_path);
          fd_ = fd;
          dev_ = dev;
          break;
        }
      } catch (const errno_error& e) {
        // ENOTTY just means the device isn't an evdev device, skip it
        if (e.code != ENOTTY) {
          spdlog::warn(e.what());
        }
      }
      closeFile(fd);
    }
    if (dev_ == nullptr) {
      throw errno_error(errno, "Failed to find keyboard device");
    }
  }

  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

waybar::modules::KeyboardState::~KeyboardState() {
  libevdev_free(dev_);
  try {
    closeFile(fd_);
  } catch (const std::runtime_error& e) {
    spdlog::warn(e.what());
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
  if (-err != EAGAIN) {
    throw errno_error(-err, "Failed to sync evdev device");
  }

  int numl = libevdev_get_event_value(dev_, EV_LED, LED_NUML);
  int capsl = libevdev_get_event_value(dev_, EV_LED, LED_CAPSL);
  int scrolll = libevdev_get_event_value(dev_, EV_LED, LED_SCROLLL);

  struct {
    bool state;
    Gtk::Label& label;
    const std::string& format;
    const char* name;
  } label_states[] = {
      {(bool)numl, numlock_label_, numlock_format_, "Num"},
      {(bool)capsl, capslock_label_, capslock_format_, "Caps"},
      {(bool)scrolll, scrolllock_label_, scrolllock_format_, "Scroll"},
  };
  for (auto& label_state : label_states) {
    std::string text;
    text = fmt::format(label_state.format,
                       fmt::arg("icon", label_state.state ? icon_locked_ : icon_unlocked_),
                       fmt::arg("name", label_state.name));
    label_state.label.set_markup(text);
    if (label_state.state) {
      label_state.label.get_style_context()->add_class("locked");
    } else {
      label_state.label.get_style_context()->remove_class("locked");
    }
  }

  AModule::update();
}
