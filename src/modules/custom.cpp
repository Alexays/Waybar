#include "modules/custom.hpp"

waybar::modules::Custom::Custom(const std::string name,
  const Json::Value& config)
  : ALabel(config), name_(name)
{
  if (!config_["exec"]) {
    throw std::runtime_error(name_ + " has no exec path.");
  }
  label_.set_name("custom-" + name_);
  worker();
}

void waybar::modules::Custom::worker()
{
  uint32_t interval = config_["interval"] ? config_["inveral"].asUInt() : 30;
  thread_ = [this, interval] {
    bool can_update = true;
    if (config_["exec-if"]) {
      auto res = waybar::util::command::exec(config_["exec-if"].asString());
      if (res.exit_code != 0) {
        can_update = false;
        label_.hide();
      }
    }
    if (can_update) {
      dp.emit();
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
  } else {
    auto format = config_["format"] ? config_["format"].asString() : "{}";
    auto str = fmt::format(format, res.out);
    label_.set_text(str);
    label_.set_tooltip_text(str);
    label_.show();
  }
}
