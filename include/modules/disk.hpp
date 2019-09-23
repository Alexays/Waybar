#pragma once

#include <fmt/format.h>
#include <fstream>
#include <sys/statvfs.h>
#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"
#include "util/format.hpp"

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
