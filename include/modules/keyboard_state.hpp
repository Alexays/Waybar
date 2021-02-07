#pragma once

#include <fmt/format.h>
#if FMT_VERSION < 60000
#include <fmt/time.h>
#else
#include <fmt/chrono.h>
#endif
#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

extern "C" {
#include <libevdev/libevdev.h>
}

namespace waybar::modules {

class KeyboardState : public ALabel {
 public:
  KeyboardState(const std::string&, const Json::Value&);
  ~KeyboardState();
  auto update() -> void;

 private:
  std::string dev_path_;
  int         fd_;
  libevdev*   dev_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
