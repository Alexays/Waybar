#include "modules/backlight.hpp"

#include <fmt/format.h>
#include <libudev.h>
#include <spdlog/spdlog.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <memory>

#include "util/backend_common.hpp"
#include "util/backlight_backend.hpp"

waybar::modules::Backlight::Backlight(const std::string &id, const Json::Value &config)
    : ALabel(config, "backlight", id, "{percent}%", 2),
      preferred_device_(config["device"].isString() ? config["device"].asString() : ""),
      backend(interval_, [this] { dp.emit(); }) {
  dp.emit();

  // Set up scroll handler
  event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &Backlight::handleScroll));
}

auto waybar::modules::Backlight::update() -> void {
  GET_BEST_DEVICE(best, backend, preferred_device_);

  const auto previous_best_device = backend.get_previous_best_device();
  if (best != nullptr) {
    if (previous_best_device != nullptr && *previous_best_device == *best &&
        !previous_format_.empty() && previous_format_ == format_) {
      return;
    }

    if (best->get_powered()) {
      event_box_.show();
      const uint8_t percent =
          best->get_max() == 0 ? 100 : round(best->get_actual() * 100.0f / best->get_max());
      std::string desc = fmt::format(fmt::runtime(format_), fmt::arg("percent", percent),
                                     fmt::arg("icon", getIcon(percent)));
      label_.set_markup(desc);
      getState(percent);
      if (tooltipEnabled()) {
        std::string tooltip_format;
        if (config_["tooltip-format"].isString()) {
          tooltip_format = config_["tooltip-format"].asString();
        }
        if (!tooltip_format.empty()) {
          label_.set_tooltip_text(fmt::format(fmt::runtime(tooltip_format),
                                              fmt::arg("percent", percent),
                                              fmt::arg("icon", getIcon(percent))));
        } else {
          label_.set_tooltip_text(desc);
        }
      }
    } else {
      event_box_.hide();
    }
  } else {
    if (previous_best_device == nullptr) {
      return;
    }
    label_.set_markup("");
  }
  backend.set_previous_best_device(best);
  previous_format_ = format_;
  ALabel::update();
}

bool waybar::modules::Backlight::handleScroll(GdkEventScroll *e) {
  // Check if the user has set a custom command for scrolling
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScroll(e);
  }

  // Fail fast if the proxy could not be initialized
  if (!backend.is_login_proxy_initialized()) {
    return true;
  }

  // Check scroll direction
  auto dir = AModule::getScrollDir(e);

  // No worries, it will always be set because of the switch below. This is purely to suppress a
  // warning
  util::ChangeType ct = util::ChangeType::Increase;

  switch (dir) {
    case SCROLL_DIR::UP:
      [[fallthrough]];
    case SCROLL_DIR::RIGHT:
      ct = util::ChangeType::Increase;
      break;

    case SCROLL_DIR::DOWN:
      [[fallthrough]];
    case SCROLL_DIR::LEFT:
      ct = util::ChangeType::Decrease;
      break;

    case SCROLL_DIR::NONE:
      return true;
      break;
  }

  // Get scroll step
  double step = 1;

  if (config_["scroll-step"].isDouble()) {
    step = config_["scroll-step"].asDouble();
  }

  double min_brightness = 0;
  if (config_["min-brightness"].isDouble()) {
    min_brightness = config_["min-brightness"].asDouble();
  }
  if (backend.get_scaled_brightness(preferred_device_) <= min_brightness &&
      ct == util::ChangeType::Decrease) {
    return true;
  }
  backend.set_brightness(preferred_device_, ct, step);

  return true;
}
