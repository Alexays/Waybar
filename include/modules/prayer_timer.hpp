#pragma once
#include <fmt/chrono.h>
#include "ALabel.hpp"
#include <fstream>
#include "util/sleeper_thread.hpp"
namespace waybar::modules {

class prayer_timer : public ALabel {
 public:
  prayer_timer(const std::string&, const Json::Value&);
  virtual ~prayer_timer() = default;
  auto update() -> void override;
  void SecondsToString(int,char*,int);
 private:
  util::SleeperThread thread_;
  std::string file_path_;
};

}  // namespace waybar::modules
