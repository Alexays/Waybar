#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "client.hpp"
#include "util/chrono.hpp"
#include "util/json.hpp"
#include "ALabel.hpp"
#include "modules/sway/ipc/client.hpp"

namespace waybar::modules::sway {

class Mode : public ALabel {
  public:
    Mode(const waybar::Bar&, const Json::Value&);
    auto update() -> void;
  private:
    void worker();
    
    const Bar& bar_;
    waybar::util::SleeperThread thread_;
    util::JsonParser parser_;
    Ipc ipc_;
    std::string mode_;
};

}