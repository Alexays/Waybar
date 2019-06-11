#include "ALabel.hpp"
#include <fmt/format.h>
#include <util/command.hpp>

waybar::ALabel::ALabel(const Json::Value& config, const std::string& name, const std::string& id,
                       const std::string& format, uint16_t interval)
    : config_(config),
      format_(config_["format"].isString() ? config_["format"].asString() : format),
      interval_(config_["interval"] == "once"
                    ? std::chrono::seconds(100000000)
                    : std::chrono::seconds(
                          config_["interval"].isUInt() ? config_["interval"].asUInt() : interval)),
      default_format_(format_) {
  label_.set_name(name);
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  event_box_.add(label_);
  if (config_["max-length"].isUInt()) {
    label_.set_max_width_chars(config_["max-length"].asUInt());
    label_.set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
  }

  if (config_["rotate"].isUInt()) {
    label_.set_angle(config["rotate"].asUInt());
  }

  if (config_["format-alt"].isString()) {
    event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
    event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &ALabel::handleToggle));
  }

  // configure events' user commands
  if (config_["on-click"].isString() || config_["on-click-middle"].isString() ||
      config_["on-click-backward"].isString() || config_["on-click-forward"].isString() ||
      config_["on-click-right"].isString()) {
    event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
    event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &ALabel::handleToggle));
  }
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &ALabel::handleScroll));
  }
}

waybar::ALabel::~ALabel() {
  for (const auto& pid : pid_) {
    if (pid != -1) {
      kill(-pid, 9);
    }
  }
}

auto waybar::ALabel::update() -> void {
  // Nothing here
}

bool waybar::ALabel::handleToggle(GdkEventButton* const& e) {
  std::string format;
  if (config_["on-click"].isString() && e->button == 1) {
    format = config_["on-click"].asString();
  } else if (config_["on-click-middle"].isString() && e->button == 2) {
    format = config_["on-click-middle"].asString();
  } else if (config_["on-click-right"].isString() && e->button == 3) {
    format = config_["on-click-right"].asString();
  } else if (config_["on-click-forward"].isString() && e->button == 8) {
    format = config_["on-click-backward"].asString();
  } else if (config_["on-click-backward"].isString() && e->button == 9) {
    format = config_["on-click-forward"].asString();
  }
  if (!format.empty()) {
    pid_.push_back(util::command::forkExec(fmt::format(format, fmt::arg("arg", click_param))));
  }
  if (config_["format-alt-click"].isUInt() && e->button == config_["format-alt-click"].asUInt()) {
    alt_ = !alt_;
    if (alt_ && config_["format-alt"].isString()) {
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
  std::lock_guard<std::mutex> lock(mutex_);
  bool                        direction_up = false;

  if (e->direction == GDK_SCROLL_UP) {
    direction_up = true;
  }
  if (e->direction == GDK_SCROLL_DOWN) {
    direction_up = false;
  }
  if (e->direction == GDK_SCROLL_SMOOTH) {
    gdouble delta_x, delta_y;
    gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent *>(e), &delta_x, &delta_y);
    distance_scrolled_ += delta_y;
    gdouble threshold = 0;
    if (config_["smooth-scrolling-threshold"].isNumeric()) {
      threshold = config_["smooth-scrolling-threshold"].asDouble();
    }

    if (distance_scrolled_ < -threshold) {
      direction_up = true;
    } else if (distance_scrolled_ > threshold) {
      direction_up = false;
    }
    if(abs(distance_scrolled_) > threshold) {
      distance_scrolled_ = 0;
    } else {
      // Don't execute the action if we haven't met the threshold!
      return false;
    }
  }
  if (direction_up && config_["on-scroll-up"].isString()) {
    pid_.push_back(util::command::forkExec(config_["on-scroll-up"].asString()));
  } else if (config_["on-scroll-down"].isString()) {
    pid_.push_back(util::command::forkExec(config_["on-scroll-down"].asString()));
  }
  dp.emit();
  return true;
}

std::string waybar::ALabel::getIcon(uint16_t percentage, const std::string& alt, uint16_t max) {
  auto format_icons = config_["format-icons"];
  if (format_icons.isObject()) {
    if (!alt.empty() && (format_icons[alt].isString() || format_icons[alt].isArray())) {
      format_icons = format_icons[alt];
    } else {
      format_icons = format_icons["default"];
    }
  }
  if (format_icons.isArray()) {
    auto size = format_icons.size();
    auto idx = std::clamp(percentage / ((max == 0 ? 100 : max) / size), 0U, size - 1);
    format_icons = format_icons[idx];
  }
  if (format_icons.isString()) {
    return format_icons.asString();
  }
  return "";
}

std::string waybar::ALabel::getState(uint8_t value, bool lesser) {
  if (!config_["states"].isObject()) {
    return "";
  }
  // Get current state
  std::vector<std::pair<std::string, uint8_t>> states;
  if (config_["states"].isObject()) {
    for (auto it = config_["states"].begin(); it != config_["states"].end(); ++it) {
      if (it->isUInt() && it.key().isString()) {
        states.emplace_back(it.key().asString(), it->asUInt());
      }
    }
  }
  // Sort states
  std::sort(states.begin(), states.end(), [&lesser](auto& a, auto& b) {
    return lesser ? a.second < b.second : a.second > b.second;
  });
  std::string valid_state;
  for (auto const& state : states) {
    if ((lesser ? value <= state.second : value >= state.second) && valid_state.empty()) {
      label_.get_style_context()->add_class(state.first);
      valid_state = state.first;
    } else {
      label_.get_style_context()->remove_class(state.first);
    }
  }
  return valid_state;
}

bool waybar::ALabel::tooltipEnabled() {
  return config_["tooltip"].isBool() ? config_["tooltip"].asBool() : true;
}

waybar::ALabel::operator Gtk::Widget&() { return event_box_; }
