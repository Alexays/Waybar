#include "modules/custom.hpp"
#include <iostream>

waybar::modules::Custom::Custom(std::string name, Json::Value config)
  : _name(name), _config(config)
{
  if (!_config["exec"]) {
    std::cerr << name + " has no exec path." << std::endl;
    return;
  }
  int interval = _config["interval"] ? _config["inveral"].asInt() : 30;
  _thread = [this, interval] {
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Custom::update));
    _thread.sleep_for(chrono::seconds(interval));
  };
};

auto waybar::modules::Custom::update() -> void
{
  std::array<char, 128> buffer;
  std::string output;
  std::shared_ptr<FILE> fp(popen(_config["exec"].asCString(), "r"), pclose);
  if (!fp) {
    std::cerr << _name + " can't exec " + _config["exec"].asString() << std::endl;
    return;
  }
  while (!feof(fp.get())) {
    if (fgets(buffer.data(), 128, fp.get()) != nullptr)
        output += buffer.data();
  }

  // Remove last newline
  if (!output.empty() && output[output.length()-1] == '\n') {
    output.erase(output.length()-1);
  }

  // Hide label if output is empty
  if (output.empty()) {
    _label.get_style_context()->remove_class("custom-" + _name);
    _label.hide();
  } else {
    _label.get_style_context()->add_class("custom-" + _name);
    auto format = _config["format"] ? _config["format"].asString() : "{}";
    _label.set_text(fmt::format(format, output));
    _label.show();
  }
}

waybar::modules::Custom::operator Gtk::Widget &() {
  return _label;
}
