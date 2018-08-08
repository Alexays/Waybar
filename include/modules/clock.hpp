#pragma once

#include <gtkmm.h>
#include <fmt/format.h>
#include <thread>
#include "util/chrono.hpp"

namespace waybar::modules {

  class Clock {
    public:
      Clock();
      operator Gtk::Widget &();
    private:
      Gtk::Label _label;
      waybar::util::SleeperThread _thread;
  };

}
