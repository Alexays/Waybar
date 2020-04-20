#pragma once

#ifdef FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
#else
#include <filesystem>
#endif
#include <sys/inotify.h>

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
  Battery(const std::string &, const Json::Value &);
  ~Battery();
  auto update(std::string format, fmt::dynamic_format_arg_store<fmt::format_context> &args)
      -> void override;

 private:
  static inline const fs::path data_dir_ = "/sys/class/power_supply/";

  void getBatteries();
  void worker();
  const std::string getAdapterStatus(uint8_t capacity) const;
  const std::tuple<uint8_t, float, std::string> getInfos() const;
  const std::string formatTimeRemaining(float hoursRemaining) const;

  std::vector<fs::path> batteries_;
  fs::path adapter_;
  int fd_;
  std::vector<int> wds_;
  std::string status_;

  util::SleeperThread thread_;
  util::SleeperThread thread_timer_;
};

}  // namespace waybar::modules
