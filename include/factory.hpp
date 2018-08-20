#pragma once

#include <json/json.h>
#include "modules/clock.hpp"
#include "modules/sway/workspaces.hpp"
#include "modules/sway/window.hpp"
#include "modules/battery.hpp"
#include "modules/memory.hpp"
#include "modules/cpu.hpp"
#include "modules/network.hpp"
#include "modules/pulseaudio.hpp"
#include "modules/custom.hpp"

namespace waybar {

class Factory {
  public:
    Factory(Bar& bar, const Json::Value& config);
    IModule* makeModule(const std::string &name) const;
  private:
    Bar& bar_;
    const Json::Value& config_;
};

}
