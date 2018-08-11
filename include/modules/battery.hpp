#pragma once

#include <json/json.h>
#include <filesystem>
#include <fstream>
#include <gtkmm.h>
#include <iostream>
#include <fmt/format.h>
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
      std::string _getIcon(uint32_t percentage);
      static inline const fs::path _data_dir = "/sys/class/power_supply/";
      std::vector<fs::path> _batteries;
      util::SleeperThread _thread;
      Gtk::Label _label;
      Json::Value _config;
  };

}
