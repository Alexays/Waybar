#pragma once

#include <fmt/format.h>
#include <sys/statvfs.h>

#include <fstream>

#include "ALabel.hpp"
#include "util/format.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Disk : public ALabel {
 public:
  Disk(const std::string&, const Json::Value&);
  ~Disk() = default;
  auto update() -> void;

 private:
  util::SleeperThread thread_;
  std::string path_;
};

}  // namespace waybar::modules
