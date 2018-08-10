#pragma once

#include <json/json.h>
#include <gtkmm.h>
#include <fmt/format.h>
#include <thread>
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

  class Custom : public IModule {
    public:
      Custom(std::string name, Json::Value config);
      auto update() -> void;
      operator Gtk::Widget &();
    private:
      Gtk::Label _label;
      waybar::util::SleeperThread _thread;
      const std::string _name;
      Json::Value _config;
  };

}
