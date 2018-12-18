#pragma once

#ifdef FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
#else
#include <filesystem>
#endif
#include <fstream>
#include <iostream>
#include <fmt/format.h>
#include <sys/inotify.h>
#include <algorithm>
#include "util/chrono.hpp"
#include "ALabel.hpp"

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
  
    void worker();
    const std::tuple<uint8_t, std::string> getInfos() const;
    const std::string getState(uint8_t) const;

    util::SleeperThread thread_;
    util::SleeperThread thread_timer_;
    std::vector<fs::path> batteries_;
    int fd_;
    std::string old_status_;
};

}
