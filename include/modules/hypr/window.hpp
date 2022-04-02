#pragma once

#include <fmt/format.h>
#include <fstream>
#include <sys/statvfs.h>
#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"
#include "util/format.hpp"
#include "ipc.hpp"

namespace waybar::modules::hypr {

class Window : public ALabel {
 public:
  Window(const std::string&, const Json::Value&);
  ~Window() = default;
  auto update() -> void;

 private:
  util::SleeperThread thread_;
};

}  // namespace waybar::modules::hypr
