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
      void _updateThread();
      static void _handle_idle(void *data,
        struct org_kde_kwin_idle_timeout *timer);
      static void _handle_resume(void *data,
        struct org_kde_kwin_idle_timeout *timer);
      void _addWorkspace(Json::Value node);
      Json::Value _getWorkspaces();
      Bar &_bar;
      util::SleeperThread *_thread;
      Gtk::Box *_box;
      std::unordered_map<int, Gtk::Button> _buttons;
      int _ipcSocketfd;
      int _ipcEventSocketfd;
      struct org_kde_kwin_idle_timeout *_idle_timer;
  };

}
