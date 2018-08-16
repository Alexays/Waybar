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
    Window(waybar::Bar&, Json::Value);
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    std::string getFocusedNode(Json::Value nodes);
    void getFocusedWindow();

    Bar& bar_;
    Json::Value config_;
    waybar::util::SleeperThread thread_;
    Gtk::Label label_;
    util::JsonParser parser_;
    int ipcfd_;
    int ipc_eventfd_;
    std::string window_;
};

}
