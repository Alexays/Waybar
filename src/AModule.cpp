#include "AModule.hpp"
#include <fmt/format.h>
#include <util/command.hpp>

namespace waybar {

AModule::AModule(const Json::Value& config, const std::string& name, const std::string& id,
                 bool enable_click, bool enable_scroll)
    : config_(std::move(config)) {
  // configure events' user commands
  if (config_["on-click"].isString() || config_["on-click-middle"].isString() ||
      config_["on-click-backward"].isString() || config_["on-click-forward"].isString() ||
      config_["on-click-right"].isString() || enable_click) {
    event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
    event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &AModule::handleToggle));
  }
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString() || enable_scroll) {
    event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &AModule::handleScroll));
  }
}

AModule::~AModule() {
  for (const auto& pid : pid_) {
    if (pid != -1) {
      kill(-pid, 9);
    }
  }
}

auto AModule::update() -> void {
  // Nothing here
}

bool AModule::handleToggle(GdkEventButton* const& e) {
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
    pid_.push_back(util::command::forkExec(format));
  }
  dp.emit();
  return true;
}

AModule::SCROLL_DIR AModule::getScrollDir(GdkEventScroll* e) {
  switch (e -> direction) {
    case GDK_SCROLL_UP: return SCROLL_DIR::UP;
    case GDK_SCROLL_DOWN: return SCROLL_DIR::DOWN;
    case GDK_SCROLL_LEFT: return SCROLL_DIR::LEFT;
    case GDK_SCROLL_RIGHT: return SCROLL_DIR::RIGHT;
    case GDK_SCROLL_SMOOTH: {
      SCROLL_DIR dir{SCROLL_DIR::NONE};

      distance_scrolled_y_ += e->delta_y;
      distance_scrolled_x_ += e->delta_x;

      gdouble threshold = 0;
      if (config_["smooth-scrolling-threshold"].isNumeric()) {
        threshold = config_["smooth-scrolling-threshold"].asDouble();
      }

      if (distance_scrolled_y_ < -threshold) {
        dir = SCROLL_DIR::UP;
      } else if (distance_scrolled_y_ > threshold) {
        dir = SCROLL_DIR::DOWN;
      } else if (distance_scrolled_x_ > threshold) {
        dir = SCROLL_DIR::RIGHT;
      } else if (distance_scrolled_x_ < -threshold) {
        dir = SCROLL_DIR::LEFT;
      }

      switch (dir) {
        case SCROLL_DIR::UP:
        case SCROLL_DIR::DOWN:
          distance_scrolled_y_ = 0;
          break;
        case SCROLL_DIR::LEFT:
        case SCROLL_DIR::RIGHT:
          distance_scrolled_x_ = 0;
          break;
        case SCROLL_DIR::NONE:
          break;
      }

      return dir;
    }
    // Silence -Wreturn-type:
    default: return SCROLL_DIR::NONE;
  }
}

bool AModule::handleScroll(GdkEventScroll* e) {
  auto dir = getScrollDir(e);
  if (dir == SCROLL_DIR::UP && config_["on-scroll-up"].isString()) {
    pid_.push_back(util::command::forkExec(config_["on-scroll-up"].asString()));
  } else if (dir == SCROLL_DIR::DOWN && config_["on-scroll-down"].isString()) {
    pid_.push_back(util::command::forkExec(config_["on-scroll-down"].asString()));
  }
  dp.emit();
  return true;
}

bool AModule::tooltipEnabled() {
  return config_["tooltip"].isBool() ? config_["tooltip"].asBool() : true;
}

AModule::operator Gtk::Widget&() { return event_box_; }

}  // namespace waybar
