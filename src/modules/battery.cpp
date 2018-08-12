#include "modules/battery.hpp"

waybar::modules::Battery::Battery(Json::Value config)
  : _config(config)
{
  try {
    for (auto &node : fs::directory_iterator(_data_dir)) {
      if (fs::is_directory(node) && fs::exists(node / "capacity")
        && fs::exists(node / "status")) {
        _batteries.push_back(node);
      }
    }
  } catch (fs::filesystem_error &e) {
    std::cerr << e.what() << std::endl;
  }

  if (!_batteries.size()) {
    std::cerr << "No batteries." << std::endl;
    return;
  }

  auto fd = inotify_init();
  for (auto &bat : _batteries) {
    int capacity = inotify_add_watch(fd, (bat / "capacity").c_str(), IN_ACCESS);
    int status = inotify_add_watch(fd, (bat / "status").c_str(), IN_ACCESS);
    _wd.emplace(capacity, &Battery::_handleCapacity);
    _wd.emplace(status, &Battery::_handleStatus);
  }
  // Get first value
  for (auto& node : _wd)
    (this->*node.second)();
  update();
  _label.get_style_context()->add_class("battery");
  _thread = [this, fd] {
    struct inotify_event event;
    int nbytes = read(fd, &event, sizeof(event));
    if (nbytes != sizeof(event))
      return;
    (this->*_wd[event.wd])();
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Battery::update));
  };
}

auto waybar::modules::Battery::update() -> void
{
  try {
    auto format = _config["format"] ? _config["format"].asString() : "{}%";
    _label.set_text(fmt::format(format, fmt::arg("value", _capacity),
      fmt::arg("icon", _getIcon(_capacity))));
    bool charging = _status == "Charging";
    _label.set_tooltip_text(_status);
    if (charging)
      _label.get_style_context()->add_class("charging");
    else
      _label.get_style_context()->remove_class("charging");
    if (_capacity < 16 && !charging)
      _label.get_style_context()->add_class("warning");
    else
      _label.get_style_context()->remove_class("warning");
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

void waybar::modules::Battery::_handleCapacity()
{
  uint16_t total = 0;
  for (auto &bat : _batteries) {
    uint16_t capacity;
    std::ifstream(bat / "capacity") >> capacity;
    total += capacity;
  }
  _capacity = total / _batteries.size();
}

void waybar::modules::Battery::_handleStatus()
{
  std::string status;
  for (auto &bat : _batteries) {
    std::ifstream(bat / "status") >> status;
    if (status == "Charging") {
      _status = status;
      return;
    }
  }
  _status = status;
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
