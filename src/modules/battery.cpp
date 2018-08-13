#include "modules/battery.hpp"

waybar::modules::Battery::Battery(Json::Value config)
  : _config(config)
{
  try {
    for (auto &node : fs::directory_iterator(_data_dir)) {
      if (fs::is_directory(node) && fs::exists(node / "capacity")
        && fs::exists(node / "status") && fs::exists(node / "uevent"))
        _batteries.push_back(node);
    }
  } catch (fs::filesystem_error &e) {
    throw std::runtime_error(e.what());
  }

  if (!_batteries.size())
    throw std::runtime_error("No batteries.");

  auto fd = inotify_init();
  if (fd == -1)
    throw std::runtime_error("Unable to listen batteries.");
  for (auto &bat : _batteries)
    inotify_add_watch(fd, (bat / "uevent").c_str(), IN_ACCESS);
  // Trigger first value
  update();
  _label.get_style_context()->add_class("battery");
  _thread = [this, fd] {
    struct inotify_event event;
    int nbytes = read(fd, &event, sizeof(event));
    if (nbytes != sizeof(event))
      return;
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Battery::update));
  };
}

auto waybar::modules::Battery::update() -> void
{
  try {
    uint16_t total = 0;
    bool charging = false;
    std::string status;
    for (auto &bat : _batteries) {
      uint16_t capacity;
      std::ifstream(bat / "capacity") >> capacity;
      std::ifstream(bat / "status") >> status;
      if (status == "Charging")
        charging = true;
      total += capacity;
    }
    uint16_t capacity = total / _batteries.size();
    auto format = _config["format"] ? _config["format"].asString() : "{}%";
    _label.set_text(fmt::format(format, fmt::arg("value", capacity),
      fmt::arg("icon", _getIcon(capacity))));
    _label.set_tooltip_text(status);
    if (charging)
      _label.get_style_context()->add_class("charging");
    else
      _label.get_style_context()->remove_class("charging");
    if (capacity < 16 && !charging)
      _label.get_style_context()->add_class("warning");
    else
      _label.get_style_context()->remove_class("warning");
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

std::string waybar::modules::Battery::_getIcon(uint16_t percentage)
{
  if (!_config["format-icons"] || !_config["format-icons"].isArray()) return "";
  auto step = 100 / _config["format-icons"].size();
  return _config["format-icons"][percentage / step].asString();
}

waybar::modules::Battery::operator Gtk::Widget &()
{
  return _label;
}
