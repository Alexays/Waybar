#pragma once

#include <gtkmm.h>
#include <fmt/format.h>
#include <thread>
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

  class Clock : public IModule {
    public:
      Clock();
      auto update() -> void;
      operator Gtk::Widget &();
    private:
      Gtk::Label _label;
      waybar::util::SleeperThread _thread;
  };

}
