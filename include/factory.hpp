#pragma once

#include <json/json.h>
#include "modules/clock.hpp"
#ifdef HAVE_SWAY
#include "modules/sway/mode.hpp"
#include "modules/sway/window.hpp"
#include "modules/sway/workspaces.hpp"
#endif
#ifdef HAVE_WLR
#include "modules/wlr/taskbar.hpp"
#endif
#ifdef HAVE_RIVER
#include "modules/river/tags.hpp"
#endif
#if defined(__linux__) && !defined(NO_FILESYSTEM)
#include "modules/battery.hpp"
#endif
#if defined(HAVE_CPU_LINUX) || defined(HAVE_CPU_BSD)
#include "modules/cpu.hpp"
#endif
#include "modules/idle_inhibitor.hpp"
#if defined(HAVE_MEMORY_LINUX) || defined(HAVE_MEMORY_BSD)
#include "modules/memory.hpp"
#endif
#include "modules/disk.hpp"
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
#ifdef HAVE_LIBSNDIO
#include "modules/sndio.hpp"
#endif
#include "bar.hpp"
#include "modules/custom.hpp"
#include "modules/temperature.hpp"
#if defined(__linux__)
#  ifdef WANT_RFKILL
#    include "modules/bluetooth.hpp"
#  endif
#endif

namespace waybar {

class Factory {
 public:
  Factory(const Bar& bar, const Json::Value& config);
  AModule* makeModule(const std::string& name) const;

 private:
  const Bar&         bar_;
  const Json::Value& config_;
};

}  // namespace waybar
