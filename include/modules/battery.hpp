#pragma once

#include <fmt/format.h>

#include <filesystem>
#if defined(__linux__)
#include <sys/inotify.h>
#endif
#include <sys/poll.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "ALabel.hpp"
#include "bar.hpp"
#include "util/sleeper_thread.hpp"
#include "util/udev_deleter.hpp"

namespace waybar::modules {

namespace fs = std::filesystem;

class Battery : public ALabel {
 public:
  Battery(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~Battery();
  auto update() -> void override;

 private:
  static inline const fs::path data_dir_ = "/sys/class/power_supply/";

  void refreshBatteries();
  void worker();
  const std::string getAdapterStatus(uint8_t capacity) const;
  std::tuple<uint8_t, float, std::string, float, uint16_t, float> getInfos();
  const std::string formatTimeRemaining(float hoursRemaining);
  void setBarClass(std::string&);
  void processEvents(std::string& state, std::string& status, uint8_t capacity);

  std::map<fs::path, int> batteries_;
  std::unique_ptr<udev, util::UdevDeleter> udev_;
  std::array<pollfd, 1> poll_fds_;
  std::unique_ptr<udev_monitor, util::UdevMonitorDeleter> mon_;
  fs::path adapter_;
  int battery_watch_fd_;
  std::mutex battery_list_mutex_;
  std::string old_status_;
  std::string last_event_;
  bool warnFirstTime_{true};
  bool weightedAverage_{true};
  const Bar& bar_;

  util::SleeperThread thread_;
  util::SleeperThread thread_battery_update_;
  util::SleeperThread thread_timer_;
};

}  // namespace waybar::modules
