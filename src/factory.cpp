#include "factory.hpp"

waybar::Factory::Factory(const Bar& bar, const Json::Value& config) : bar_(bar), config_(config) {}

waybar::AModule* waybar::Factory::makeModule(const std::string& name) const {
  try {
    auto hash_pos = name.find('#');
    auto ref = name.substr(0, hash_pos);
    auto id = hash_pos != std::string::npos ? name.substr(hash_pos + 1) : "";
#if defined(__FreeBSD__) || (defined(__linux__) && !defined(NO_FILESYSTEM))
    if (ref == "battery") {
      return new waybar::modules::Battery(id, config_[name]);
    }
#endif
#ifdef HAVE_GAMEMODE
    if (ref == "gamemode") {
      return new waybar::modules::Gamemode(id, config_[name]);
    }
#endif
#ifdef HAVE_UPOWER
    if (ref == "upower") {
      return new waybar::modules::upower::UPower(id, config_[name]);
    }
#endif
#ifdef HAVE_MPRIS
    if (ref == "mpris") {
      return new waybar::modules::mpris::Mpris(id, config_[name]);
    }
#endif
#ifdef HAVE_SWAY
    if (ref == "sway/mode") {
      return new waybar::modules::sway::Mode(id, config_[name]);
    }
    if (ref == "sway/workspaces") {
      return new waybar::modules::sway::Workspaces(id, bar_, config_[name]);
    }
    if (ref == "sway/window") {
      return new waybar::modules::sway::Window(id, bar_, config_[name]);
    }
    if (ref == "sway/language") {
      return new waybar::modules::sway::Language(id, config_[name]);
    }
    if (ref == "sway/scratchpad") {
      return new waybar::modules::sway::Scratchpad(id, config_[name]);
    }
#endif
#ifdef HAVE_WLR
    if (ref == "wlr/taskbar") {
      return new waybar::modules::wlr::Taskbar(id, bar_, config_[name]);
    }
#ifdef USE_EXPERIMENTAL
    if (ref == "wlr/workspaces") {
      return new waybar::modules::wlr::WorkspaceManager(id, bar_, config_[name]);
    }
#endif
#endif
#ifdef HAVE_RIVER
    if (ref == "river/mode") {
      return new waybar::modules::river::Mode(id, bar_, config_[name]);
    }
    if (ref == "river/tags") {
      return new waybar::modules::river::Tags(id, bar_, config_[name]);
    }
    if (ref == "river/window") {
      return new waybar::modules::river::Window(id, bar_, config_[name]);
    }
    if (ref == "river/layout") {
      return new waybar::modules::river::Layout(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_DWL
    if (ref == "dwl/tags") {
      return new waybar::modules::dwl::Tags(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_HYPRLAND
    if (ref == "hyprland/window") {
      return new waybar::modules::hyprland::Window(id, bar_, config_[name]);
    }
    if (ref == "hyprland/language") {
      return new waybar::modules::hyprland::Language(id, bar_, config_[name]);
    }
    if (ref == "hyprland/submap") {
      return new waybar::modules::hyprland::Submap(id, bar_, config_[name]);
    }
    if (ref == "hyprland/workspaces") {
      return new waybar::modules::hyprland::Workspaces(id, bar_, config_[name]);
    }
#endif
    if (ref == "idle_inhibitor") {
      return new waybar::modules::IdleInhibitor(id, bar_, config_[name]);
    }
#if defined(HAVE_MEMORY_LINUX) || defined(HAVE_MEMORY_BSD)
    if (ref == "memory") {
      return new waybar::modules::Memory(id, config_[name]);
    }
#endif
#if defined(HAVE_CPU_LINUX) || defined(HAVE_CPU_BSD)
    if (ref == "cpu") {
      return new waybar::modules::Cpu(id, config_[name]);
    }
#endif
    if (ref == "clock") {
      return new waybar::modules::Clock(id, config_[name]);
    }
    if (ref == "user") {
      return new waybar::modules::User(id, config_[name]);
    }
    if (ref == "disk") {
      return new waybar::modules::Disk(id, config_[name]);
    }
    if (ref == "image") {
      return new waybar::modules::Image(id, config_[name]);
    }
#ifdef HAVE_DBUSMENU
    if (ref == "tray") {
      return new waybar::modules::SNI::Tray(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_LIBNL
    if (ref == "network") {
      return new waybar::modules::Network(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBUDEV
    if (ref == "backlight") {
      return new waybar::modules::Backlight(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBEVDEV
    if (ref == "keyboard-state") {
      return new waybar::modules::KeyboardState(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_LIBPULSE
    if (ref == "pulseaudio") {
      return new waybar::modules::Pulseaudio(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBMPDCLIENT
    if (ref == "mpd") {
      return new waybar::modules::MPD(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBSNDIO
    if (ref == "sndio") {
      return new waybar::modules::Sndio(id, config_[name]);
    }
#endif
#ifdef HAVE_GIO_UNIX
    if (ref == "bluetooth") {
      return new waybar::modules::Bluetooth(id, config_[name]);
    }
    if (ref == "inhibitor") {
      return new waybar::modules::Inhibitor(id, bar_, config_[name]);
    }
#endif
#ifdef HAVE_LIBJACK
    if (ref == "jack") {
      return new waybar::modules::JACK(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBWIREPLUMBER
    if (ref == "wireplumber") {
      return new waybar::modules::Wireplumber(id, config_[name]);
    }
#endif
#ifdef HAVE_LIBCAVA
    if (ref == "cava") {
      return new waybar::modules::Cava(id, config_[name]);
    }
#endif
    if (ref == "temperature") {
      return new waybar::modules::Temperature(id, config_[name]);
    }
    if (ref.compare(0, 7, "custom/") == 0 && ref.size() > 7) {
      return new waybar::modules::Custom(ref.substr(7), id, config_[name]);
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
