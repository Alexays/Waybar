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
      void updateThread();
      operator Gtk::Widget &();
      util::SleeperThread *thread;
    private:
      void _addWorkspace(Json::Value node);
      Json::Value _getWorkspaces();
      Bar &_bar;
      Gtk::Box *_box;
      std::unordered_map<int, Gtk::Button> _buttons;
      int _ipcSocketfd;
      int _ipcEventSocketfd;
      struct org_kde_kwin_idle_timeout *_idle_timer;
  };

}
