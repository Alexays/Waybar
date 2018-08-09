#pragma once

#include <json/json.h>
#include "modules/clock.hpp"
#include "modules/workspaces.hpp"
#include "modules/battery.hpp"
#include "modules/memory.hpp"
#include "modules/cpu.hpp"

namespace waybar {

  class Factory {
	  public:
      Factory(Bar &bar, Json::Value config);
      IModule &makeModule(std::string name);
    private:
      Bar &_bar;
      Json::Value _config;
  };

}
