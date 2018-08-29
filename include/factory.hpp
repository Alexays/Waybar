#pragma once

#include <json/json.h>
#include "modules/clock.hpp"
#ifdef HAVE_SWAY
#include "modules/sway/workspaces.hpp"
#include "modules/sway/window.hpp"
#endif
#include "modules/battery.hpp"
#include "modules/memory.hpp"
#include "modules/cpu.hpp"
#include "modules/sni/tray.hpp"
#ifdef HAVE_LIBNL
#include "modules/network.hpp"
#endif
#ifdef HAVE_LIBPULSE
#include "modules/pulseaudio.hpp"
#endif
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
