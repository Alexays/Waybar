#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "client.hpp"
#include "util/chrono.hpp"
#include "util/json.hpp"
#include "IModule.hpp"

namespace waybar::modules {

  class Workspaces : public IModule {
    public:
      Workspaces(waybar::Bar &bar, Json::Value config);
      auto update() -> void;
      operator Gtk::Widget &();
    private:
      void _addWorkspace(Json::Value node);
      std::string _getIcon(std::string name);
      Json::Value _getWorkspaces(const std::string data);
      bool _handleScroll(GdkEventScroll *e);
      int _getPrevWorkspace();
      int _getNextWorkspace();
      Bar &_bar;
      Json::Value _config;
      waybar::util::SleeperThread _thread;
      Gtk::Box _box;
      util::JsonParser _parser;
      std::mutex _mutex;
      bool _scrolling;
      std::unordered_map<int, Gtk::Button> _buttons;
      Json::Value _workspaces;
      int _ipcfd;
      int _ipcEventfd;
  };

}
