#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "client.hpp"
#include "util/chrono.hpp"
#include "util/json.hpp"
#include "IModule.hpp"

namespace waybar::modules::sway {

  class Window : public IModule {
    public:
      Window(waybar::Bar &bar, Json::Value config);
      auto update() -> void;
      operator Gtk::Widget &();
    private:
      std::string _getFocusedNode(Json::Value nodes);
      void _getFocusedWindow();
      Bar &_bar;
      Json::Value _config;
      waybar::util::SleeperThread _thread;
      Gtk::Label _label;
      util::JsonParser _parser;
      int _ipcfd;
      int _ipcEventfd;
      std::string _window;
  };

}
