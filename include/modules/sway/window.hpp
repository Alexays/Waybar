#pragma once

#include <fmt/format.h>
#include <tuple>
#include "bar.hpp"
#include "client.hpp"
#include "util/chrono.hpp"
#include "util/json.hpp"
#include "ALabel.hpp"
#include "modules/sway/ipc/client.hpp"

namespace waybar::modules::sway {

class Window : public ALabel {
  public:
    Window(const std::string&, const waybar::Bar&, const Json::Value&);
    auto update() -> void;
  private:
    void worker();
    std::tuple<int, std::string> getFocusedNode(Json::Value nodes);
    void getFocusedWindow();

    const Bar& bar_;
    waybar::util::SleeperThread thread_;
    util::JsonParser parser_;
    Ipc ipc_;
    std::string window_;
    int windowId_;
};

}
