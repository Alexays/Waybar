#include "modules/custom.hpp"

waybar::modules::Custom::Custom(const std::string name,
  const Json::Value& config)
  : ALabel(config, "{}"), name_(name)
{
  if (!config_["exec"]) {
    throw std::runtime_error(name_ + " has no exec path.");
  }
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
        label_.set_name("");
      }
    }
    if (can_update) {
      output_ = waybar::util::command::exec(config_["exec"].asString());
      dp.emit();
    }
    thread_.sleep_for(chrono::seconds(interval));
  };
}

auto waybar::modules::Custom::update() -> void
{
  // Hide label if output is empty
  if (output_.out.empty() || output_.exit_code != 0) {
    label_.hide();
    label_.set_name("");
  } else {
    label_.set_name("custom-" + name_);
    auto str = fmt::format(format_, output_.out);
    label_.set_text(str);
    label_.set_tooltip_text(str);
    label_.show();
  }
}