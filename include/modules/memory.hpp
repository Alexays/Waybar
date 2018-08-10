#pragma once

#include <json/json.h>
#include <gtkmm.h>
#include <fmt/format.h>
#include <sys/sysinfo.h>
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

  class Memory : public IModule {
    public:
      Memory(Json::Value config);
      auto update() -> void;
      operator Gtk::Widget &();
    private:
      Gtk::Label _label;
      waybar::util::SleeperThread _thread;
      Json::Value _config;
  };

}
