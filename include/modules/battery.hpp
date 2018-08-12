#pragma once

#include <json/json.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <fmt/format.h>
#include <sys/inotify.h>
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

  namespace fs = std::filesystem;

  class Battery : public IModule {
    public:
      Battery(Json::Value config);
      auto update() -> void;
      operator Gtk::Widget&();
    private:
      void _handleCapacity();
      void _handleStatus();
      std::string _getIcon(uint16_t percentage);
      static inline const fs::path _data_dir = "/sys/class/power_supply/";
      std::vector<fs::path> _batteries;
      util::SleeperThread _thread;
      Gtk::Label _label;
      Json::Value _config;
      typedef void(Battery::*func)(void);
      std::unordered_map<int, func> _wd;
      std::string _status;
      uint16_t _capacity;
  };

}
