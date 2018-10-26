#include "ALabel.hpp"

#include <iostream>

waybar::ALabel::ALabel(const Json::Value& config, const std::string format)
  : config_(config),
    format_(config_["format"].isString() ? config_["format"].asString() : format),
    default_format_(format_)
{
  event_box_.add(label_);
	if (config_["max-length"].isUInt()) {
    label_.set_max_width_chars(config_["max-length"].asUInt());
    label_.set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
  }
  if (config_["format-alt"].isString()) {
    event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
    event_box_.signal_button_press_event()
      .connect(sigc::mem_fun(*this, &ALabel::handleToggle));
  }
}

auto waybar::ALabel::update() -> void
{
  // Nothing here
}

bool waybar::ALabel::handleToggle(GdkEventButton* const& /*ev*/)
{
  alt = !alt;
  if (alt) {
    format_ = config_["format-alt"].asString();
  } else {
    format_ = default_format_;
  }
  dp.emit();
  return true;
}

std::string waybar::ALabel::getIcon(uint16_t percentage, const std::string& alt)
{
  auto format_icons = config_["format-icons"];
  if (format_icons.isObject()) {
    if (!alt.empty() && format_icons[alt]) {
      format_icons = format_icons[alt];
    } else {
      format_icons = format_icons["default"];
    }
  }
  if (format_icons.isArray()) {
    auto size = format_icons.size();
    auto idx = std::clamp(percentage / (100 / size), 0U, size - 1);
    format_icons = format_icons[idx];
  }
  if (format_icons.isString()) {
    return format_icons.asString();
  }
  return "";
}

waybar::ALabel::operator Gtk::Widget &() {
  return event_box_;
}
