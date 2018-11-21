#include "modules/sway/mode.hpp"

waybar::modules::sway::Mode::Mode(Bar& bar, const Json::Value& config)
  : ALabel(config, "{}"), bar_(bar)
{
  ipc_.connect();
  ipc_.subscribe("[ \"mode\" ]");
  // Launch worker
  worker();
}

void waybar::modules::sway::Mode::worker()
{
  thread_ = [this] {
    try {
      auto res = ipc_.handleEvent();
      auto parsed = parser_.parse(res.payload);
      if ((parsed["change"]) != "default" ) {
        mode_ = parsed["change"].asString();
        dp.emit();
      }
      else if ((parsed["change"]) == "default" ) {
        mode_.clear();
        dp.emit();
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  };
}

auto waybar::modules::sway::Mode::update() -> void
{
  if (mode_.empty()) {
    label_.set_name("");
    label_.hide();
  } else {
    label_.set_name("mode");
    label_.set_markup(fmt::format(format_, mode_));
    label_.set_tooltip_text(mode_);
    label_.show();
  }
}