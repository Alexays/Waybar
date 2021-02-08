#pragma once

#include <fmt/format.h>
#if FMT_VERSION < 60000
#include <fmt/time.h>
#else
#include <fmt/chrono.h>
#endif
#include "AModule.hpp"
#include "bar.hpp"
#include "util/sleeper_thread.hpp"
#include <gtkmm/label.h>

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
  static auto openDevice(const std::string&) -> std::pair<int, libevdev*>;

  const Bar&  bar_;
  Gtk::Box    box_;
  Gtk::Label  numlock_label_;
  Gtk::Label  capslock_label_;
  Gtk::Label  scrolllock_label_;

  std::string numlock_format_;
  std::string capslock_format_;
  std::string scrolllock_format_;
  const std::chrono::seconds interval_;
  std::string icon_locked_;
  std::string icon_unlocked_;

  int         fd_;
  libevdev*   dev_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
