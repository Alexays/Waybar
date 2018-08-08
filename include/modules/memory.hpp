#pragma once

#include <gtkmm.h>
#include <fmt/format.h>
#include <sys/sysinfo.h>
#include <thread>
#include "util/chrono.hpp"

namespace waybar::modules {

  class Memory {
    public:
      Memory();
      operator Gtk::Widget &();
    private:
      Gtk::Label _label;
      waybar::util::SleeperThread _thread;
  };

}
