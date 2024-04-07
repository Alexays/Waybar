#include "factory.hpp"

#include "bar.hpp"

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
#ifdef HAVE_WLR_TASKBAR
#include "modules/wlr/taskbar.hpp"
#endif
#ifdef HAVE_WLR_WORKSPACES
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
#include "modules/dwl/window.hpp"
#endif
#ifdef HAVE_HYPRLAND
#include "modules/hyprland/language.hpp"
#include "modules/hyprland/submap.hpp"
#include "modules/hyprland/window.hpp"
#include "modules/hyprland/workspaces.hpp"
#endif
#if defined(__FreeBSD__) || defined(__linux__)
#include "modules/battery.hpp"
#endif
#if defined(HAVE_CPU_LINUX) || defined(HAVE_CPU_BSD)
#include "modules/cpu.hpp"
#include "modules/cpu_frequency.hpp"
#include "modules/cpu_usage.hpp"
#include "modules/load.hpp"
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
#include "modules/backlight_slider.hpp"
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
#ifdef HAVE_PIPEWIRE
#include "modules/privacy/privacy.hpp"
#endif
#ifdef HAVE_LIBPULSE
#include "modules/pulseaudio.hpp"
#include "modules/pulseaudio_slider.hpp"
#endif
#ifdef HAVE_LIBMPDCLIENT
#include "modules/mpd/mpd.hpp"
#endif
#ifdef HAVE_LIBSNDIO
#include "modules/sndio.hpp"
#endif
#if defined(__linux__)
#include "modules/bluetooth.hpp"
#include "modules/power_profiles_daemon.hpp"
#endif
#ifdef HAVE_LOGIND_INHIBITOR
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
#ifdef HAVE_SYSTEMD_MONITOR
#include "modules/systemd_failed_units.hpp"
#endif
#include "modules/cffi.hpp"
#include "modules/custom.hpp"
#include "modules/image.hpp"
#include "modules/temperature.hpp"
#include "modules/user.hpp"

wabar::Factory::Factory(const Bar& bar, const Json::Value& config) : bar_(bar), config_(config) {}

wabar::AModule* wabar::Factory::makeModule(const std::string& name,
                                             const std::string& pos) const {
  try {
    auto hash_pos = name.find('#');
    auto ref = name.substr(0, hash_pos);
    auto id = hash_pos != std::string::npos ? name.substr(hash_pos + 1) : "";
#if defined(__FreeBSD__) || defined(__linux__)
    if (ref == "battery") {
      return new wabar::modules::Battery(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_GAMEMODE
    if (ref == "gamemode") {
      return new wabar::modules::Gamemode(id, config_[name]);
    }
#endif
#ifdef HAVE_UPOWER
    if (ref == "upower") {
      return new wabar::modules::upower::UPower(id, config_[name]);
    }
#endif
#ifdef HAVE_PIPEWIRE
    if (ref == "privacy") {
      return new wabar::modules::privacy::Privacy(id, config_[name], pos);
    }
#endif
#ifdef HAVE_MPRIS
    if (ref == "mpris") {
      return new wabar::modules::mpris::Mpris(id, config_[name]);
    }
#endif
#ifdef HAVE_SWAY
    if (ref == "sway/mode") {
      return new wabar::modules::sway::Mode(id, config_[name]);
    }
    if (ref == "sway/workspaces") {
      return new wabar::modules::sway::Workspaces(id, bar_, config_[name]);
    }
    if (ref == "sway/window") {
      return new wabar::modules::sway::Window(id, bar_, config_[name]);
    }
    if (ref == "sway/language") {
      return new wabar::modules::sway::Language(id, config_[name]);
    }
    if (ref == "sway/scratchpad") {
      return new wabar::modules::sway::Scratchpad(id, config_[name]);
    }
#endif
#ifdef HAVE_WLR_TASKBAR
    if (ref == "wlr/taskbar") {
      return new wabar::modules::wlr::Taskbar(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_WLR_WORKSPACES
    if (ref == "wlr/workspaces") {
      return new wabar::modules::wlr::WorkspaceManager(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_RIVER
    if (ref == "river/mode") {
      return new wabar::modules::river::Mode(id, bar_, config_[name]);
    }
    if (ref == "river/tags") {
      return new wabar::modules::river::Tags(id, bar_, config_[name]);
    }
    if (ref == "river/window") {
      return new wabar::modules::river::Window(id, bar_, config_[name]);
    }
    if (ref == "river/layout") {
      return new wabar::modules::river::Layout(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_DWL
    if (ref == "dwl/tags") {
      return new wabar::modules::dwl::Tags(id, bar_, config_[name]);
    }
    if (ref == "dwl/window") {
      return new wabar::modules::dwl::Window(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_HYPRLAND
    if (ref == "hyprland/window") {
      return new wabar::modules::hyprland::Window(id, bar_, config_[name]);
    }
    if (ref == "hyprland/language") {
      return new wabar::modules::hyprland::Language(id, bar_, config_[name]);
    }
    if (ref == "hyprland/submap") {
      return new wabar::modules::hyprland::Submap(id, bar_, config_[name]);
    }
    if (ref == "hyprland/workspaces") {
      return new wabar::modules::hyprland::Workspaces(id, bar_, config_[name]);
    }
#endif
    if (ref == "idle_inhibitor") {
      return new wabar::modules::IdleInhibitor(id, bar_, config_[name]);
    }
#if defined(HAVE_MEMORY_LINUX) || defined(HAVE_MEMORY_BSD)
    if (ref == "memory") {
      return new wabar::modules::Memory(id, config_[name]);
    }
#endif
#if defined(HAVE_CPU_LINUX) || defined(HAVE_CPU_BSD)
    if (ref == "cpu") {
      return new wabar::modules::Cpu(id, config_[name]);
    }
#if defined(HAVE_CPU_LINUX)
    if (ref == "cpu_frequency") {
      return new wabar::modules::CpuFrequency(id, config_[name]);
    }
#endif
    if (ref == "cpu_usage") {
      return new wabar::modules::CpuUsage(id, config_[name]);
    }
    if (ref == "load") {
      return new wabar::modules::Load(id, config_[name]);
    }
#endif
    if (ref == "clock") {
      return new wabar::modules::Clock(id, config_[name]);
    }
    if (ref == "user") {
      return new wabar::modules::User(id, config_[name]);
    }
    if (ref == "disk") {
      return new wabar::modules::Disk(id, config_[name]);
    }
    if (ref == "image") {
      return new wabar::modules::Image(id, config_[name]);
    }
#ifdef HAVE_DBUSMENU
    if (ref == "tray") {
      return new wabar::modules::SNI::Tray(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_LIBNL
    if (ref == "network") {
      return new wabar::modules::Network(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBUDEV
    if (ref == "backlight") {
      return new wabar::modules::Backlight(id, config_[name]);
    }
    if (ref == "backlight/slider") {
      return new wabar::modules::BacklightSlider(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBEVDEV
    if (ref == "keyboard-state") {
      return new wabar::modules::KeyboardState(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_LIBPULSE
    if (ref == "pulseaudio") {
      return new wabar::modules::Pulseaudio(id, config_[name]);
    }
    if (ref == "pulseaudio/slider") {
      return new wabar::modules::PulseaudioSlider(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBMPDCLIENT
    if (ref == "mpd") {
      return new wabar::modules::MPD(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBSNDIO
    if (ref == "sndio") {
      return new wabar::modules::Sndio(id, config_[name]);
    }
#endif
#if defined(__linux__)
    if (ref == "bluetooth") {
      return new wabar::modules::Bluetooth(id, config_[name]);
    }
    if (ref == "power-profiles-daemon") {
      return new wabar::modules::PowerProfilesDaemon(id, config_[name]);
    }
#endif
#ifdef HAVE_LOGIND_INHIBITOR
    if (ref == "inhibitor") {
      return new wabar::modules::Inhibitor(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_LIBJACK
    if (ref == "jack") {
      return new wabar::modules::JACK(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBWIREPLUMBER
    if (ref == "wireplumber") {
      return new wabar::modules::Wireplumber(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBCAVA
    if (ref == "cava") {
      return new wabar::modules::Cava(id, config_[name]);
    }
#endif
#ifdef HAVE_SYSTEMD_MONITOR
    if (ref == "systemd-failed-units") {
      return new wabar::modules::SystemdFailedUnits(id, config_[name]);
    }
#endif
    if (ref == "temperature") {
      return new wabar::modules::Temperature(id, config_[name]);
    }
    if (ref.compare(0, 7, "custom/") == 0 && ref.size() > 7) {
      return new wabar::modules::Custom(ref.substr(7), id, config_[name], bar_.output->name);
    }
    if (ref.compare(0, 5, "cffi/") == 0 && ref.size() > 5) {
      return new wabar::modules::CFFI(ref.substr(5), id, config_[name]);
    }
  } catch (const std::exception& e) {
    auto err = fmt::format("Disabling module \"{}\", {}", name, e.what());
    throw std::runtime_error(err);
  } catch (...) {
    auto err = fmt::format("Disabling module \"{}\", Unknown reason", name);
    throw std::runtime_error(err);
  }
  throw std::runtime_error("Unknown module: " + name);
}
