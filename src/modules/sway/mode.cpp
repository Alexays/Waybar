#include "modules/sway/mode.hpp"

waybar::modules::sway::Mode::Mode(const std::string& id, const Bar& bar, const Json::Value& config)
  : ALabel(config, "{}"), bar_(bar)
{
  label_.set_name("mode");
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  ipc_.subscribe("[ \"mode\" ]");
  // Launch worker
  worker();
  dp.emit();
}

void waybar::modules::sway::Mode::worker()
{
  thread_ = [this] {
    try {
      auto res = ipc_.handleEvent();
      auto parsed = parser_.parse(res.payload);
      if (parsed["change"] != "default") {
        mode_ = parsed["change"].asString();
      } else {
        mode_.clear();
      }
      dp.emit();
    } catch (const std::exception& e) {
      std::cerr << "Mode: " << e.what() << std::endl;
    }
  };
}

auto waybar::modules::sway::Mode::update() -> void
{
  if (mode_.empty()) {
    event_box_.hide();
  } else {
    label_.set_markup(fmt::format(format_, mode_));
    label_.set_tooltip_text(mode_);
    event_box_.show();
  }
}