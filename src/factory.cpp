#include "factory.hpp"

waybar::Factory::Factory(const Bar& bar, const Json::Value& config)
  : bar_(bar), config_(config)
{}

waybar::IModule* waybar::Factory::makeModule(const std::string &name) const
{
  try {
    auto hash_pos = name.find("#");
    auto ref = name.substr(0, hash_pos);
    auto id = hash_pos != std::string::npos ? name.substr(hash_pos + 1) : "";
    if (ref == "battery") {
      return new waybar::modules::Battery(id, config_[name]);
    }
    #ifdef HAVE_SWAY
    if (ref == "sway/mode") {
      return new waybar::modules::sway::Mode(id, bar_, config_[name]);
    }
    if (ref == "sway/workspaces") {
      return new waybar::modules::sway::Workspaces(id, bar_, config_[name]);
    }
    if (ref == "sway/window") {
      return new waybar::modules::sway::Window(id, bar_, config_[name]);
    }
    #endif
    if (ref == "memory") {
      return new waybar::modules::Memory(id, config_[name]);
    }
    if (ref == "cpu") {
      return new waybar::modules::Cpu(id, config_[name]);
    }
    if (ref == "clock") {
      return new waybar::modules::Clock(id, config_[name]);
    }
    #ifdef HAVE_DBUSMENU
    if (ref == "tray") {
      return new waybar::modules::SNI::Tray(id, config_[name]);
    }
    #endif
    #ifdef HAVE_LIBNL
    if (ref == "network") {
      return new waybar::modules::Network(id, config_[name]);
    }
    #endif
    #ifdef HAVE_LIBPULSE
    if (ref == "pulseaudio") {
      return new waybar::modules::Pulseaudio(id, config_[name]);
    }
    #endif
    #ifdef HAVE_LIGHT
    if (ref == "light") {
      return new waybar::modules::Light(id, config_[name]);
    }
    #endif
    if (ref.compare(0, 7, "custom/") == 0 && ref.size() > 7) {
      return new waybar::modules::Custom(ref.substr(7), config_[name]);
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
