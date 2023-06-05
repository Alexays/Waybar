#include "modules/clock.hpp"

waybar::modules::Clock::Clock(const std::string &id, const Json::Value &config)
    : ALabel(config, "clock", id, "{:%H:%M}", 60, false, false, false), DBusClient("clock") {
  thread_ = [this] {
    dp.emit();
    auto now = std::chrono::system_clock::now();
    /* difference with projected wakeup time */
    auto diff = now.time_since_epoch() % interval_;
    /* sleep until the next projected time */
    thread_.sleep_for(interval_ - diff);
  };
}

auto waybar::modules::Clock::resultCallback() -> void {
  if (label_.get_label() != resLabel || label_.get_tooltip_markup() != resTooltip) {
    if (label_.get_label() != resLabel) label_.set_markup(resLabel);
    if (label_.get_tooltip_markup() != resTooltip) label_.set_tooltip_markup(resTooltip);
    ALabel::update();
  }
}

auto waybar::modules::Clock::update() -> void { doAction(resultMap_.cbegin()->first); }

auto waybar::modules::Clock::doAction(const Glib::ustring &name) -> void {
  DBusClient::doAction(name);
}
