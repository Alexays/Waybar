#pragma once

#include <filesystem>
#if defined(__linux__)
#include <sys/inotify.h>
#endif

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

namespace fs = std::filesystem;

class Battery final : public ALabel {
 public:
  Battery(const std::string&, const Json::Value&);
  virtual ~Battery();
  auto update() -> void override;

 private:
  static inline const fs::path data_dir_{"/sys/class/power_supply/"};

  void refreshBatteries();
  void worker();
  const std::string getAdapterStatus(uint8_t capacity) const;
  std::tuple<uint8_t, float, std::string, float, uint16_t, float> getInfos();
  const std::string formatTimeRemaining(float hoursRemaining);
  void setBarClass(std::string&);

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
