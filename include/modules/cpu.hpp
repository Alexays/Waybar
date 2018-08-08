#pragma once

#include <gtkmm.h>
#include <fmt/format.h>
#include <sys/sysinfo.h>
#include <thread>
#include "util/chrono.hpp"

namespace waybar::modules {

  class Cpu {
    public:
      Cpu();
      operator Gtk::Widget &();
    private:
      Gtk::Label _label;
      waybar::util::SleeperThread _thread;
  };

}
