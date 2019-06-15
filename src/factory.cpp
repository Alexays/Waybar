#include "factory.hpp"

waybar::Factory::Factory(const Bar& bar, const Json::Value& config) : bar_(bar), config_(config) {}

waybar::AModule* waybar::Factory::makeModule(const std::string& name) const {
  try {
    auto hash_pos = name.find('#');
    auto ref = name.substr(0, hash_pos);
    auto id = hash_pos != std::string::npos ? name.substr(hash_pos + 1) : "";
#ifndef NO_FILESYSTEM
    if (ref == "battery") {
      return new waybar::modules::Battery(id, config_[name]);
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
#endif
    if (ref == "idle_inhibitor") {
      return new waybar::modules::IdleInhibitor(id, bar_, config_[name]);
    }
    if (ref == "memory") {
      return new waybar::modules::Memory(id, config_[name]);
    }
    if (ref == "cpu") {
      return new waybar::modules::Cpu(id, config_[name]);
    }
    if (ref == "clock") {
      return new waybar::modules::Clock(id, config_[name]);
    }
#if defined(HAVE_DBUSMENU) && !defined(NO_FILESYSTEM)
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
