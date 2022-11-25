#pragma once

#ifdef FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
#else
#include <filesystem>
#endif
#include <fmt/format.h>
#if defined(__linux__)
#include <sys/inotify.h>
#endif

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

#ifdef FILESYSTEM_EXPERIMENTAL
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

class Battery : public ALabel {
 public:
  Battery(const std::string&, const Json::Value&);
  ~Battery();
  auto update() -> void;

 private:
  static inline const fs::path data_dir_ = "/sys/class/power_supply/";

  void refreshBatteries();
  void worker();
  const std::string getAdapterStatus(uint8_t capacity) const;
  const std::tuple<uint8_t, float, std::string, float> getInfos();
  const std::string formatTimeRemaining(float hoursRemaining);

  int global_watch;
  std::map<fs::path, int> batteries_;
  fs::path adapter_;
  int battery_watch_fd_;
  int global_watch_fd_;
  std::mutex battery_list_mutex_;
  std::string old_status_;
  bool warnFirstTime_{true};

  util::SleeperThread thread_;
  util::SleeperThread thread_battery_update_;
  util::SleeperThread thread_timer_;
};

}  // namespace waybar::modules
