#include "factory.hpp"

waybar::Factory::Factory(Bar& bar, const Json::Value& config)
  : bar_(bar), config_(config)
{}

waybar::IModule* waybar::Factory::makeModule(const std::string &name) const
{
  try {
    if (name == "battery") {
      return new waybar::modules::Battery(config_[name]);
    }
    #ifdef HAVE_SWAY
    if (name == "sway/workspaces") {
      return new waybar::modules::sway::Workspaces(bar_, config_[name]);
    }
    #endif
    if (name == "sway/window") {
      return new waybar::modules::sway::Window(bar_, config_[name]);
    }
    if (name == "memory") {
      return new waybar::modules::Memory(config_[name]);
    }
    if (name == "cpu") {
      return new waybar::modules::Cpu(config_[name]);
    }
    if (name == "clock") {
      return new waybar::modules::Clock(config_[name]);
    }
    if (name == "tray") {
      return new waybar::modules::SNI::Tray(config_[name]);
    }
    #ifdef HAVE_LIBNL
    if (name == "network") {
      return new waybar::modules::Network(config_[name]);
    }
    #endif
    #ifdef HAVE_LIBPULSE
    if (name == "pulseaudio") {
      return new waybar::modules::Pulseaudio(config_[name]);
    }
    #endif
    if (name.compare(0, 7, "custom/") == 0 && name.size() > 7) {
      return new waybar::modules::Custom(name.substr(7), config_[name]);
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
