#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <fmt/format.h>
#include <sys/inotify.h>
#include <algorithm>
#include "util/chrono.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

namespace fs = std::filesystem;

class Battery : public ALabel {
  public:
    Battery(const Json::Value&);
    ~Battery();
    auto update() -> void;
  private:
    static inline const fs::path data_dir_ = "/sys/class/power_supply/";
  
    void worker();
    std::tuple<uint16_t, std::string> getInfos();
    std::string getState(uint16_t, bool);

    util::SleeperThread thread_;
    util::SleeperThread threadTimer_;
    std::vector<fs::path> batteries_;
    int fd_;
    std::string old_state_;
};

}
