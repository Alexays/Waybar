#include "modules/keyboard_state.hpp"

#include <errno.h>
#include <spdlog/spdlog.h>
#include <string.h>

#include <filesystem>

extern "C" {
#include <fcntl.h>
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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
      devices_path_("/dev/input/"),
      libinput_(nullptr),
      libinput_devices_({}) {
  static struct libinput_interface interface = {
      [](const char* path, int flags, void* user_data) { return open(path, flags); },
      [](int fd, void* user_data) { close(fd); }};
  if (config_["interval"].isUInt()) {
    spdlog::warn("keyboard-state: interval is deprecated");
  }

  libinput_ = libinput_path_create_context(&interface, NULL);

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
    tryAddDevice(dev_path);
    if (libinput_devices_.empty()) {
      spdlog::error("keyboard-state: Cannot find device {}", dev_path);
    }
  }

  DIR* dev_dir = opendir(devices_path_.c_str());
  if (dev_dir == nullptr) {
    throw errno_error(errno, "Failed to open " + devices_path_);
  }
  dirent* ep;
  while ((ep = readdir(dev_dir))) {
    if (ep->d_type == DT_DIR) continue;
    std::string dev_path = devices_path_ + ep->d_name;
    tryAddDevice(dev_path);
  }

  if (libinput_devices_.empty()) {
    throw errno_error(errno, "Failed to find keyboard device");
  }

  libinput_thread_ = [this] {
    dp.emit();
    while (1) {
      struct pollfd fd = {libinput_get_fd(libinput_), POLLIN, 0};
      poll(&fd, 1, -1);
      libinput_dispatch(libinput_);
      struct libinput_event* event;
      while ((event = libinput_get_event(libinput_))) {
        auto type = libinput_event_get_type(event);
        if (type == LIBINPUT_EVENT_KEYBOARD_KEY) {
          auto keyboard_event = libinput_event_get_keyboard_event(event);
          auto state = libinput_event_keyboard_get_key_state(keyboard_event);
          if (state == LIBINPUT_KEY_STATE_RELEASED) {
            uint32_t key = libinput_event_keyboard_get_key(keyboard_event);
            switch (key) {
              case KEY_CAPSLOCK:
              case KEY_NUMLOCK:
              case KEY_SCROLLLOCK:
                dp.emit();
                break;
              default:
                break;
            }
          }
        }
        libinput_event_destroy(event);
      }
    }
  };

  hotplug_thread_ = [this] {
    int fd;
    fd = inotify_init();
    if (fd < 0) {
      spdlog::error("Failed to initialize inotify: {}", strerror(errno));
      return;
    }
    inotify_add_watch(fd, devices_path_.c_str(), IN_CREATE | IN_DELETE);
    while (1) {
      int BUF_LEN = 1024 * (sizeof(struct inotify_event) + 16);
      char buf[BUF_LEN];
      int length = read(fd, buf, 1024);
      if (length < 0) {
        spdlog::error("Failed to read inotify: {}", strerror(errno));
        return;
      }
      for (int i = 0; i < length;) {
        struct inotify_event* event = (struct inotify_event*)&buf[i];
        std::string dev_path = devices_path_ + event->name;
        if (event->mask & IN_CREATE) {
          // Wait for device setup
          int timeout = 10;
          while (timeout--) {
            try {
              int fd = openFile(dev_path, O_NONBLOCK | O_CLOEXEC | O_RDONLY);
              closeFile(fd);
              break;
            } catch (const errno_error& e) {
              if (e.code == EACCES) {
                sleep(1);
              }
            }
          }
          tryAddDevice(dev_path);
        } else if (event->mask & IN_DELETE) {
          auto it = libinput_devices_.find(dev_path);
          if (it != libinput_devices_.end()) {
            spdlog::info("Keyboard {} has been removed.", dev_path);
            libinput_devices_.erase(it);
          }
        }
        i += sizeof(struct inotify_event) + event->len;
      }
    }
  };
}

waybar::modules::KeyboardState::~KeyboardState() {
  for (const auto& [_, dev_ptr] : libinput_devices_) {
    libinput_path_remove_device(dev_ptr);
  }
}

auto waybar::modules::KeyboardState::update() -> void {
  sleep(0);  // Wait for keyboard status change
  int numl = 0, capsl = 0, scrolll = 0;

  try {
    std::string dev_path;
    if (config_["device-path"].isString() &&
        libinput_devices_.find(config_["device-path"].asString()) != libinput_devices_.end()) {
      dev_path = config_["device-path"].asString();
    } else {
      dev_path = libinput_devices_.begin()->first;
    }
    int fd = openFile(dev_path, O_NONBLOCK | O_CLOEXEC | O_RDONLY);
    auto dev = openDevice(fd);
    numl = libevdev_get_event_value(dev, EV_LED, LED_NUML);
    capsl = libevdev_get_event_value(dev, EV_LED, LED_CAPSL);
    scrolll = libevdev_get_event_value(dev, EV_LED, LED_SCROLLL);
    libevdev_free(dev);
    closeFile(fd);
  } catch (const errno_error& e) {
    // ENOTTY just means the device isn't an evdev device, skip it
    if (e.code != ENOTTY) {
      spdlog::warn(e.what());
    }
  }

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
    text = fmt::format(fmt::runtime(label_state.format),
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

auto waybar::modules ::KeyboardState::tryAddDevice(const std::string& dev_path) -> void {
  try {
    int fd = openFile(dev_path, O_NONBLOCK | O_CLOEXEC | O_RDONLY);
    auto dev = openDevice(fd);
    if (supportsLockStates(dev)) {
      spdlog::info("Found device {} at '{}'", libevdev_get_name(dev), dev_path);
      if (libinput_devices_.find(dev_path) == libinput_devices_.end()) {
        auto device = libinput_path_add_device(libinput_, dev_path.c_str());
        libinput_device_ref(device);
        libinput_devices_[dev_path] = device;
      }
    }
    libevdev_free(dev);
    closeFile(fd);
  } catch (const errno_error& e) {
    // ENOTTY just means the device isn't an evdev device, skip it
    if (e.code != ENOTTY) {
      spdlog::warn(e.what());
    }
  }
}
