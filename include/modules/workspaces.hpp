#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "client.hpp"
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

  class Workspaces : public IModule {
    public:
      Workspaces(waybar::Bar &bar);
      auto update() -> void;
      operator Gtk::Widget &();
    private:
      void _addWorkspace(Json::Value node);
      Json::Value _getWorkspaces();
      Bar &_bar;
      waybar::util::SleeperThread _thread;
      Gtk::Box _box;
      std::unordered_map<int, Gtk::Button> _buttons;
      int _ipcSocketfd;
      int _ipcEventSocketfd;
  };

}
