#pragma once

#include <json/json.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <fmt/format.h>
#include <sys/inotify.h>
#include <algorithm>
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

namespace fs = std::filesystem;

class Battery : public IModule {
  public:
    Battery(Json::Value);
    auto update() -> void;
    operator Gtk::Widget&();
  private:
    std::string getIcon(uint16_t percentage);

    static inline const fs::path data_dir_ = "/sys/class/power_supply/";

    Gtk::Label label_;
    Json::Value config_;
    util::SleeperThread thread_;
    std::vector<fs::path> batteries_;
};

}
