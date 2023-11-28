#pragma once

#include <unordered_set>
#ifdef FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
#else
#include <filesystem>
#endif
#include <fmt/format.h>

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
  virtual ~Battery();
  auto update() -> void override;

 private:
  static inline const fs::path data_dir_ = "/sys/class/power_supply/";

  void findBatteries();
  const std::string getAdapterStatus(uint8_t capacity) const;
  const std::tuple<uint8_t, float, std::string, float> getInfos();
  const std::string formatTimeRemaining(float hoursRemaining);

  std::unordered_set<fs::path> batteries_;
  fs::path adapter_;
  std::string old_status_;
  bool warnFirstTime_{true};

  util::SleeperThread thread_timer_;
};

}  // namespace waybar::modules
