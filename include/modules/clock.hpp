#pragma once

#include <json/json.h>
#include <gtkmm.h>
#include <fmt/format.h>
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

  class Clock : public IModule {
    public:
      Clock(Json::Value config);
      auto update() -> void;
      operator Gtk::Widget &();
    private:
      Gtk::Label _label;
      waybar::util::SleeperThread _thread;
      Json::Value _config;
  };

}
