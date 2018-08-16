#include "modules/custom.hpp"
#include <iostream>

waybar::modules::Custom::Custom(const std::string &name, Json::Value config)
  : _name(name), _config(std::move(config))
{
  if (!_config["exec"]) {
    throw std::runtime_error(name + " has no exec path.");
  }
  int interval = _config["interval"] ? _config["inveral"].asInt() : 30;
  _thread = [this, interval] {
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Custom::update));
    _thread.sleep_for(chrono::seconds(interval));
  };
};

auto waybar::modules::Custom::update() -> void
{
  std::array<char, 128> buffer = {0};
  std::string output;
  std::shared_ptr<FILE> fp(popen(_config["exec"].asCString(), "r"), pclose);
  if (!fp) {
    std::cerr << _name + " can't exec " + _config["exec"].asString() << std::endl;
    return;
  }

  while (feof(fp.get()) == 0) {
    if (fgets(buffer.data(), 128, fp.get()) != nullptr) {
        output += buffer.data();
    }
  }

  // Remove last newline
  if (!output.empty() && output[output.length()-1] == '\n') {
    output.erase(output.length()-1);
  }

  // Hide label if output is empty
  if (output.empty()) {
    _label.set_name("");
    _label.hide();
  } else {
    _label.set_name("custom-" + _name);
    auto format = _config["format"] ? _config["format"].asString() : "{}";
    _label.set_text(fmt::format(format, output));
    _label.show();
  }
}

waybar::modules::Custom::operator Gtk::Widget &() {
  return _label;
}
