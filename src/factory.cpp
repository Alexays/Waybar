#include "factory.hpp"

waybar::Factory::Factory(Bar &bar, Json::Value config)
  : _bar(bar), _config(config)
{}

waybar::IModule *waybar::Factory::makeModule(std::string name)
{
  try {
    if (name == "battery")
      return new waybar::modules::Battery(_config[name]);
    if (name == "sway/workspaces")
      return new waybar::modules::sway::Workspaces(_bar, _config[name]);
    if (name == "sway/window")
      return new waybar::modules::sway::Window(_bar, _config[name]);
    if (name == "memory")
      return new waybar::modules::Memory(_config[name]);
    if (name == "cpu")
      return new waybar::modules::Cpu(_config[name]);
    if (name == "clock")
      return new waybar::modules::Clock(_config[name]);
    if (name == "network")
      return new waybar::modules::Network(_config[name]);
    if (name == "pulseaudio")
      return new waybar::modules::Pulseaudio(_config[name]);
    if (!name.compare(0, 7, "custom/") && name.size() > 7)
      return new waybar::modules::Custom(name.substr(7), _config[name]);
    std::cerr << "Unknown module: " + name << std::endl;
  } catch (const std::exception& e) {
    auto err = fmt::format("Disabling module \"{}\", {}", name, e.what());
    std::cerr << err << std::endl;
  } catch (...) {
    auto err = fmt::format("Disabling module \"{}\", Unknown reason", name);
    std::cerr << err << std::endl;
  }
  return nullptr;
}
