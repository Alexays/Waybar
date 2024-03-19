#pragma once

#include <fmt/chrono.h>
#include <gtkmm/label.h>

#include <set>
#include <unordered_map>

#include "AModule.hpp"
#include "bar.hpp"
#include "util/sleeper_thread.hpp"

extern "C" {
#include <libevdev/libevdev.h>
#include <libinput.h>
}

namespace waybar::modules {

class KeyboardState : public AModule {
 public:
  KeyboardState(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~KeyboardState();
  auto update() -> void override;

 private:
  auto tryAddDevice(const std::string&) -> void;

  Gtk::Box box_;
  Gtk::Label numlock_label_;
  Gtk::Label capslock_label_;
  Gtk::Label scrolllock_label_;

  std::string numlock_format_;
  std::string numlock_locked_format_;
  std::string capslock_format_;
  std::string capslock_locked_format_;
  std::string scrolllock_format_;
  std::string scrolllock_locked_format_;
  std::string icon_locked_;
  std::string icon_unlocked_;
  std::string devices_path_;
  const std::chrono::seconds interval_;

  struct libinput* libinput_;
  std::unordered_map<std::string, struct libinput_device*> libinput_devices_;
  std::set<int> binding_keys;

  util::SleeperThread libinput_thread_, hotplug_thread_;
};

}  // namespace waybar::modules
