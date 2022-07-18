#pragma once

#include <fmt/chrono.h>
#include <gtkmm/label.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "util/sleeper_thread.hpp"

extern "C" {
#include <libevdev/libevdev.h>
}

namespace waybar::modules {

class KeyboardState : public AModule {
 public:
  KeyboardState(const std::string&, const waybar::Bar&, const Json::Value&);
  ~KeyboardState();
  auto update() -> void;

 private:
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

  int fd_;
  libevdev* dev_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
