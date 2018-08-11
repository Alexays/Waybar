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

  _label.get_style_context()->add_class("battery");
  int interval = _config["interval"] ? _config["inveral"].asInt() : 1;
  _thread = [this, interval] {
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Battery::update));
    _thread.sleep_for(chrono::seconds(interval));
  };
}

auto waybar::modules::Battery::update() -> void
{
  try {
    int total = 0;
    bool charging = false;
    for (auto &bat : _batteries) {
      int capacity;
      std::string status;
      std::ifstream(bat / "capacity") >> capacity;
      total += capacity;
      std::ifstream(bat / "status") >> status;
      if (status == "Charging") {
        charging = true;
      }
    }
    auto format = _config["format"] ? _config["format"].asString() : "{}%";
    auto value = total / _batteries.size();
    _label.set_text(fmt::format(format, value));
    _label.set_tooltip_text(charging ? "Charging" : "Discharging");
    if (charging)
      _label.get_style_context()->add_class("charging");
    else
      _label.get_style_context()->remove_class("charging");
    if (value < 52 && !charging)
      _label.get_style_context()->add_class("warning");
    else
      _label.get_style_context()->remove_class("warning");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

waybar::modules::Battery::operator Gtk::Widget &()
{
  return _label;
}
