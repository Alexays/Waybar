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
    Factory(Bar &bar, Json::Value config);
    IModule* makeModule(const std::string &name);
  private:
    Bar &_bar;
    Json::Value _config;
};

}
