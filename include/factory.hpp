#pragma once

#include <json/json.h>
#include "modules/clock.hpp"
#ifdef HAVE_SWAY
#include "modules/sway/mode.hpp"
#include "modules/sway/window.hpp"
#include "modules/sway/workspaces.hpp"
#endif
#include "modules/battery.hpp"
#include "modules/cpu.hpp"
#include "modules/idle_inhibitor.hpp"
#include "modules/memory.hpp"
#ifdef HAVE_DBUSMENU
#include "modules/sni/tray.hpp"
#endif
#ifdef HAVE_LIBNL
#include "modules/network.hpp"
#endif
#ifdef HAVE_LIBUDEV
#include "modules/backlight.hpp"
#endif
#ifdef HAVE_LIBPULSE
#include "modules/pulseaudio.hpp"
#endif
#ifdef HAVE_LIBMPDCLIENT
#include "modules/mpd.hpp"
#endif
#include "bar.hpp"
#include "modules/custom.hpp"
#include "modules/temperature.hpp"

namespace waybar {

class Factory {
 public:
  Factory(const Bar& bar, const Json::Value& config);
  IModule* makeModule(const std::string& name) const;

 private:
  const Bar&         bar_;
  const Json::Value& config_;
};

}  // namespace waybar
