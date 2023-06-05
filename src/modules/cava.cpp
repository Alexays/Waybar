#include "modules/cava.hpp"

waybar::modules::Cava::Cava(const std::string &id, const Json::Value &config)
    : ALabel(config, "cava", id, "{}", 1, false, false, false), DBusClient("cava") {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(frame_time_milsec_);
  };
}

auto waybar::modules::Cava::resultCallback() -> void {
  if (label_.get_label() != resLabel || label_.get_tooltip_markup() != resTooltip) {
    if (label_.get_label() != resLabel) label_.set_markup(resLabel);
    if (label_.get_tooltip_markup() != resTooltip) label_.set_tooltip_markup(resTooltip);
    ALabel::update();
  }

  if (!resFrameTime.empty())
    frame_time_milsec_ = std::chrono::milliseconds(std::stoi(resFrameTime));
}

auto waybar::modules::Cava::update() -> void { DBusClient::doAction(); }

auto waybar::modules::Cava::doAction(const Glib::ustring &name) -> void {
  DBusClient::doAction(name);
}

waybar::resultMap *const waybar::modules::Cava::getResultMap() { return &resultMap_; }
