#include "ALabel.hpp"

waybar::ALabel::ALabel(const Json::Value& config)
  : config_(config)
{
	if (config_["max-length"]) {
    label_.set_max_width_chars(config_["max-length"].asUInt());
    label_.set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
  }
}

auto waybar::ALabel::update() -> void
{
  // Nothing here
}

waybar::ALabel::operator Gtk::Widget &() {
  return label_;
}
