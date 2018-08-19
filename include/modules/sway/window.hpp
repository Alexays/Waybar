#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "client.hpp"
#include "util/chrono.hpp"
#include "util/json.hpp"
#include "ALabel.hpp"

namespace waybar::modules::sway {

class Window : public ALabel {
  public:
    Window(waybar::Bar&, Json::Value);
    ~Window();
    auto update() -> void;
  private:
    std::string getFocusedNode(Json::Value nodes);
    void getFocusedWindow();

    Bar& bar_;
    waybar::util::SleeperThread thread_;
    util::JsonParser parser_;
    int ipcfd_;
    int ipc_eventfd_;
    std::string window_;
};

}
