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

namespace waybar::modules {

Backlight::Backlight(const std::string& id, const Json::Value& config)
    : ALabel(config, "backlight", id, "{percent}%", 2, false, false, true),
      preferred_device_(config["device"].isString() ? config["device"].asString() : ""),
      backend(interval_, [this] { dp.emit(); }) {}

auto Backlight::doUpdate() -> void {
  GET_BEST_DEVICE(best, backend, preferred_device_);

  const auto previous_best_device = backend.get_previous_best_device();
  if (best != nullptr) {
    if (previous_best_device != nullptr && *previous_best_device == *best &&
        !previous_format_.empty() && previous_format_ == format_) {
      return;
    }

    if (best->get_powered()) {
      w_->show();

      const uint8_t percent =
          best->get_max() == 0 ? 100 : round(best->get_actual() * 100.0f / best->get_max());

      const uint8_t percent_exp =
          best->get_max() == 0
              ? 100
              : roundf(powf((float)best->get_actual() / best->get_max(), 1.0f / 2.718f) * 100);

      // Get the state and apply state-specific format if available
      auto state = getState(percent);
      std::string current_format = format_;
      if (!state.empty()) {
        std::string state_format_name = "format-" + state;
        if (config_[state_format_name].isString()) {
          current_format = config_[state_format_name].asString();
        }
      }

      updateLabelAndTooltip(current_format, current_format, fmt::arg("percent", percent),
                            fmt::arg("percent_exp", percent_exp),
                            fmt::arg("icon", getIcon(percent)),
                            fmt::arg("icon_exp", getIcon(percent_exp)));
    } else {
      w_->hide();
    }
  } else {
    if (previous_best_device == nullptr) {
      return;
    }
    label_.set_markup("");
  }
  backend.set_previous_best_device(best);
  previous_format_ = format_;
  ALabel::doUpdate();
}

bool Backlight::handleScroll(double dx, double dy) {
  // Check if the user has set a custom command for scrolling
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScroll(dx, dy);
  }

  // Fail fast if the proxy could not be initialized
  if (!backend.is_login_proxy_initialized()) {
    return true;
  }

  // Check scroll direction
  auto dir = AModule::getScrollDir(controller_scroll_->get_current_event());

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

}  // namespace waybar::modules
