#include "modules/battery.hpp"

waybar::modules::Battery::Battery()
{
  try {
    for (auto &node : fs::directory_iterator(_data_dir)) {
      if (fs::is_directory(node) && fs::exists(node / "charge_now") &&
          fs::exists(node / "charge_full")) {
        _batteries.push_back(node);
      }
    }
  } catch (fs::filesystem_error &e) {
    std::cerr << e.what() << std::endl;
  }

  _label.get_style_context()->add_class("battery-status");

  _thread = [this] {
    update();
    _thread.sleep_for(chrono::minutes(1));
  };
}

auto waybar::modules::Battery::update() -> void
{
  try {
    for (auto &bat : _batteries) {
      int full, now;
      std::string status;
      std::ifstream(bat / "charge_now") >> now;
      std::ifstream(bat / "charge_full") >> full;
      std::ifstream(bat / "status") >> status;
      if (status == "Charging") {
        _label.get_style_context()->add_class("battery-charging");
      } else {
        _label.get_style_context()->remove_class("battery-charging");
      }
      int pct = float(now) / float(full) * 100.f;
      _label.set_text_with_mnemonic(fmt::format("{}% {}", pct, ""));
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

waybar::modules::Battery::operator Gtk::Widget &()
{
  return _label;
}
