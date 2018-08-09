#include "factory.hpp"

waybar::Factory::Factory(Bar &bar, Json::Value config)
  : _bar(bar), _config(config)
{}

waybar::IModule &waybar::Factory::makeModule(std::string name)
{
  if (name == "battery")
    return *new waybar::modules::Battery();
  if (name == "workspaces")
    return *new waybar::modules::Workspaces(_bar);
  if (name == "memory")
    return *new waybar::modules::Memory();
  if (name == "cpu")
    return *new waybar::modules::Cpu();
  if (name == "clock")
    return *new waybar::modules::Clock();
  throw std::runtime_error("Unknown module: " + name);
}
