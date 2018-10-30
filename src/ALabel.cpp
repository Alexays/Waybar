#include "ALabel.hpp"
#include <util/command.hpp>

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
    event_box_.signal_button_press_event().connect(
        sigc::mem_fun(*this, &ALabel::handleToggle));
  }

  // configure events' user commands
  if (config_["on-click"].isString()) {
    std::string cmd = config_["on-click"].asString();
    event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
    event_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &ALabel::handleToggle));

    button_press_cmd_ = cmd;
  }
  if (config_["on-scroll-up"].isString()) {
    std::string cmd = config_["on-scroll-up"].asString();
    event_box_.add_events(Gdk::SCROLL_MASK);
    event_box_.signal_scroll_event().connect(
      sigc::mem_fun(*this, &ALabel::handleScroll));

    scroll_up_cmd_ = cmd;
  }
  if (config_["on-scroll-down"].isString()) {
    std::string cmd = config_["on-scroll-down"].asString();
    event_box_.add_events(Gdk::SCROLL_MASK);
    event_box_.signal_scroll_event().connect(
      sigc::mem_fun(*this, &ALabel::handleScroll));

    scroll_down_cmd_ = cmd;
  }
}

auto waybar::ALabel::update() -> void {
  // Nothing here
}

bool waybar::ALabel::handleToggle(GdkEventButton* const& e) {
  if (button_press_cmd_ != "" && e->button == 1) {
    waybar::util::command::forkExec(button_press_cmd_);
  } else {
    alt = !alt;
    if (alt) {
      format_ = config_["format-alt"].asString();
    } else {
      format_ = default_format_;
    }
  }

  dp.emit();
  return true;
}

bool waybar::ALabel::handleScroll(GdkEventScroll* e) {

  // Avoid concurrent scroll event
  {
    std::lock_guard<std::mutex> lock(mutex_);
    bool direction_up = false;

    if (e->direction == GDK_SCROLL_UP) {
      direction_up = true;
    }
    if (e->direction == GDK_SCROLL_DOWN) {
      direction_up = false;
    }
    if (e->direction == GDK_SCROLL_SMOOTH) {
      gdouble delta_x, delta_y;
      gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent*>(e),
                                  &delta_x, &delta_y);
      if (delta_y < 0) {
        direction_up = true;
      } else if (delta_y > 0) {
        direction_up = false;
      }
    }

    if (direction_up)
      waybar::util::command::forkExec(scroll_up_cmd_);
    else
      waybar::util::command::forkExec(scroll_down_cmd_);

    dp.emit();
  }

  return true;
}

std::string waybar::ALabel::getIcon(uint16_t percentage,
                                    const std::string& alt) {
  auto format_icons = config_["format-icons"];
  if (format_icons.isObject()) {
    if (!alt.empty() && format_icons[alt].isString()) {
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

waybar::ALabel::operator Gtk::Widget&() { return event_box_; }
