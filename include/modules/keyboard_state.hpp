#pragma once

#include <fmt/chrono.h>
#include <gtkmm/label.h>

#include <set>

#include "AModule.hpp"
#include "bar.hpp"
#include "util/sleeper_thread.hpp"

extern "C" {
#include <libinput.h>
}

namespace waybar::modules {

class KeyboardState final : public AModule {
 public:
  KeyboardState(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~KeyboardState();
  auto update() -> void override;
  Gtk::Widget& root() override;

 private:
  auto tryAddDevice(const std::string&) -> void;

  Gtk::Box box_;
  Gtk::Label numlock_label_;
  Gtk::Label capslock_label_;
  Gtk::Label scrolllock_label_;

  std::string numlock_format_;
  std::string capslock_format_;
  std::string scrolllock_format_;
  const std::chrono::seconds interval_;
  std::string icon_locked_;
  std::string icon_unlocked_;
  std::string devices_path_;

  struct libinput* libinput_;
  std::unordered_map<std::string, struct libinput_device*> libinput_devices_;
  std::set<int> binding_keys;

  util::SleeperThread libinput_thread_, hotplug_thread_;
};

}  // namespace waybar::modules
