#include "factory.hpp"

waybar::Factory::Factory(Bar &bar, Json::Value config)
  : _bar(bar), _config(config)
{}

waybar::IModule &waybar::Factory::makeModule(std::string name)
{
  if (name == "battery")
    return *new waybar::modules::Battery(_config[name]);
  if (name == "workspaces")
    return *new waybar::modules::Workspaces(_bar);
  if (name == "memory")
    return *new waybar::modules::Memory(_config[name]);
  if (name == "cpu")
    return *new waybar::modules::Cpu(_config[name]);
  if (name == "clock")
    return *new waybar::modules::Clock(_config[name]);
  if (name == "network")
    return *new waybar::modules::Network(_config[name]);
  if (name == "pulseaudio")
    return *new waybar::modules::Pulseaudio(_config[name]);
  if (!name.compare(0, 7, "custom/") && name.size() > 7)
    return *new waybar::modules::Custom(name.substr(7), _config[name]);
  throw std::runtime_error("Unknown module: " + name);
}
