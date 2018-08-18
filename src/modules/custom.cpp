#include "modules/custom.hpp"

waybar::modules::Custom::Custom(std::string name, Json::Value config)
  : ALabel(std::move(config)), name_(std::move(name))
{
  if (!config_["exec"]) {
    throw std::runtime_error(name_ + " has no exec path.");
  }
  uint32_t interval = config_["interval"] ? config_["inveral"].asUInt() : 30;
  thread_ = [this, interval] {
    bool can_update = true;
    if (config_["exec-if"]) {
      auto res = waybar::util::command::exec(config_["exec-if"].asString());
      if (res.exit_code != 0) {
        can_update = false;
      }
    }
    if (can_update) {
      Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Custom::update));
    }
    thread_.sleep_for(chrono::seconds(interval));
  };
}

auto waybar::modules::Custom::update() -> void
{
  auto res = waybar::util::command::exec(config_["exec"].asString());

  // Hide label if output is empty
  if (res.out.empty() || res.exit_code != 0) {
    label_.hide();
    label_.set_name("");
  } else {
    label_.set_name("custom-" + name_);
    auto format = config_["format"] ? config_["format"].asString() : "{}";
    auto str = fmt::format(format, res.out);
    label_.set_text(str);
    label_.set_tooltip_text(str);
    label_.show();
  }
}
