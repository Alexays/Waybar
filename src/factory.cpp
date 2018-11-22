#include "factory.hpp"

waybar::Factory::Factory(Bar& bar, const Json::Value& config)
  : bar_(bar), config_(config)
{}

waybar::IModule* waybar::Factory::makeModule(const std::string &name) const
{
  try {
    auto ref = name.substr(0, name.find("#"));
    if (ref == "battery") {
      return new waybar::modules::Battery(config_[name]);
    }
    #ifdef HAVE_SWAY
    if (ref == "sway/mode") {
      return new waybar::modules::sway::Mode(bar_, config_[name]);
    }
    if (ref == "sway/workspaces") {
      return new waybar::modules::sway::Workspaces(bar_, config_[name]);
    }
    if (ref == "sway/window") {
      return new waybar::modules::sway::Window(bar_, config_[name]);
    }
    #endif
    if (ref == "memory") {
      return new waybar::modules::Memory(config_[name]);
    }
    if (ref == "cpu") {
      return new waybar::modules::Cpu(config_[name]);
    }
    if (ref == "clock") {
      return new waybar::modules::Clock(config_[name]);
    }
    #ifdef HAVE_DBUSMENU
    if (ref == "tray") {
      return new waybar::modules::SNI::Tray(bar_, config_[name]);
    }
    #endif
    #ifdef HAVE_LIBNL
    if (ref == "network") {
      return new waybar::modules::Network(config_[name]);
    }
    #endif
    #ifdef HAVE_LIBPULSE
    if (ref == "pulseaudio") {
      return new waybar::modules::Pulseaudio(config_[name]);
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
