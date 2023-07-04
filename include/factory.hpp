#pragma once

#include <json/json.h>
#if defined(HAVE_CHRONO_TIMEZONES) || defined(HAVE_LIBDATE)
#include "modules/clock.hpp"
#else
#include "modules/simpleclock.hpp"
#endif
#ifdef HAVE_SWAY
#include "modules/sway/language.hpp"
#include "modules/sway/mode.hpp"
#include "modules/sway/scratchpad.hpp"
#include "modules/sway/window.hpp"
#include "modules/sway/workspaces.hpp"
#endif
#ifdef HAVE_WLR
#include "modules/wlr/taskbar.hpp"
#include "modules/wlr/workspace_manager.hpp"
#endif
#ifdef HAVE_RIVER
#include "modules/river/layout.hpp"
#include "modules/river/mode.hpp"
#include "modules/river/tags.hpp"
#include "modules/river/window.hpp"
#endif
#ifdef HAVE_DWL
#include "modules/dwl/tags.hpp"
#endif
#ifdef HAVE_HYPRLAND
#include "modules/hyprland/backend.hpp"
#include "modules/hyprland/language.hpp"
#include "modules/hyprland/submap.hpp"
#include "modules/hyprland/window.hpp"
#include "modules/hyprland/workspaces.hpp"
#endif
#if defined(__FreeBSD__) || (defined(__linux__) && !defined(NO_FILESYSTEM))
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
#ifdef HAVE_MPRIS
#include "modules/mpris/mpris.hpp"
#endif
#ifdef HAVE_LIBNL
#include "modules/network.hpp"
#endif
#ifdef HAVE_LIBUDEV
#include "modules/backlight.hpp"
#endif
#ifdef HAVE_LIBEVDEV
#include "modules/keyboard_state.hpp"
#endif
#ifdef HAVE_GAMEMODE
#include "modules/gamemode.hpp"
#endif
#ifdef HAVE_UPOWER
#include "modules/upower/upower.hpp"
#endif
#ifdef HAVE_LIBPULSE
#include "modules/pulseaudio.hpp"
#endif
#ifdef HAVE_LIBMPDCLIENT
#include "modules/mpd/mpd.hpp"
#endif
#ifdef HAVE_LIBSNDIO
#include "modules/sndio.hpp"
#endif
#ifdef HAVE_GIO_UNIX
#include "modules/bluetooth.hpp"
#include "modules/inhibitor.hpp"
#endif
#ifdef HAVE_LIBJACK
#include "modules/jack.hpp"
#endif
#ifdef HAVE_LIBWIREPLUMBER
#include "modules/wireplumber.hpp"
#endif
#ifdef HAVE_LIBCAVA
#include "modules/cava.hpp"
#endif
#include "bar.hpp"
#include "modules/custom.hpp"
#include "modules/image.hpp"
#include "modules/temperature.hpp"
#include "modules/user.hpp"

namespace waybar {

class Factory {
 public:
  Factory(const Bar& bar, const Json::Value& config);
  AModule* makeModule(const std::string& name) const;

 private:
  const Bar& bar_;
  const Json::Value& config_;
};

}  // namespace waybar
