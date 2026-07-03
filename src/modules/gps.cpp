#include "modules/gps.hpp"

#include <gps.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdio>

// In the 80000 version of fmt library authors decided to optimize imports
// and moved declarations required for fmt::dynamic_format_arg_store in new
// header fmt/args.h
#if (FMT_VERSION >= 80000)
#include <fmt/args.h>
#else
#include <fmt/core.h>
#endif

namespace {
using namespace waybar::util;
constexpr const char* DEFAULT_FORMAT = "{mode}";
}  // namespace

waybar::modules::Gps::Gps(const std::string& id, const Json::Value& config)
    : ALabel(config, "gps", id, "{}", 5)
#ifdef WANT_RFKILL
      ,
      rfkill_{RFKILL_TYPE_GPS}
#endif
{
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };

  if (0 != gps_open("localhost", "2947", &gps_data_)) {
    throw std::runtime_error("Can't open gpsd socket");
  }

  if (config_["hide-disconnected"].isBool()) {
    hideDisconnected = config_["hide-disconnected"].asBool();
  }

  if (config_["hide-no-fix"].isBool()) {
    hideNoFix = config_["hide-no-fix"].asBool();
  }

  gps_thread_ = [this] {
    dp.emit();
    gps_stream(&gps_data_, WATCH_ENABLE, NULL);
    int last_gps_mode = 0;

    while (gps_waiting(&gps_data_, 5000000)) {
      if (gps_read(&gps_data_, NULL, 0) == -1) {
        throw std::runtime_error("Can't read data from gpsd.");
      }

      if (MODE_SET != (MODE_SET & gps_data_.set)) {
        // did not even get mode, nothing to see here
        continue;
      }

      if (gps_data_.fix.mode != last_gps_mode) {
        // significant update
        dp.emit();
      }
      last_gps_mode = gps_data_.fix.mode;
    }
  };

#ifdef WANT_RFKILL
  rfkill_.on_update.connect(sigc::hide(sigc::mem_fun(*this, &Gps::update)));
#endif
}

const std::string waybar::modules::Gps::getFixModeName() const {
  switch (gps_data_.fix.mode) {
    case MODE_NO_FIX:
      return "fix-none";
    case MODE_2D:
      return "fix-2d";
    case MODE_3D:
      return "fix-3d";
    default:
#ifdef WANT_RFKILL
      if (rfkill_.getState()) return "disabled";
#endif
      return "disconnected";
  }
}

const std::string waybar::modules::Gps::getFixModeString() const {
  switch (gps_data_.fix.mode) {
    case MODE_NO_FIX:
      return "No fix";
    case MODE_2D:
      return "2D Fix";
    case MODE_3D:
      return "3D Fix";
    default:
      return "Disconnected";
  }
}

const std::string waybar::modules::Gps::getFixStatusString() const {
  switch (gps_data_.fix.status) {
    case STATUS_GPS:
      return "GPS";
    case STATUS_DGPS:
      return "DGPS";
    case STATUS_RTK_FIX:
      return "RTK Fixed";
    case STATUS_RTK_FLT:
      return "RTK Float";
    case STATUS_DR:
      return "Dead Reckoning";
    case STATUS_GNSSDR:
      return "GNSS + Dead Reckoning";
    case STATUS_TIME:
      return "Time Only";
    case STATUS_PPS_FIX:
      return "PPS Fix";
    default:

#ifdef WANT_RFKILL
      if (rfkill_.getState()) return "Disabled";
#endif

      return "Unknown";
  }
}

auto waybar::modules::Gps::update() -> void {
  sleep(0);  // Wait for gps status change

  if ((gps_data_.fix.mode == MODE_NOT_SEEN && hideDisconnected) ||
      (gps_data_.fix.mode == MODE_NO_FIX && hideNoFix)) {
    event_box_.set_visible(false);
    return;
  }

  // Show the module
  if (!event_box_.get_visible()) event_box_.set_visible(true);

  std::string tooltip_format;

  if (!alt_) {
    auto state = getFixModeName();
    if (!state_.empty() && label_.get_style_context()->has_class(state_)) {
      label_.get_style_context()->remove_class(state_);
    }
    if (config_["format-" + state].isString()) {
      default_format_ = config_["format-" + state].asString();
    } else if (config_["format"].isString()) {
      default_format_ = config_["format"].asString();
    } else {
      default_format_ = DEFAULT_FORMAT;
    }
    if (config_["tooltip-format-" + state].isString()) {
      tooltip_format = config_["tooltip-format-" + state].asString();
    }
    if (!label_.get_style_context()->has_class(state)) {
      label_.get_style_context()->add_class(state);
    }
    format_ = default_format_;
    state_ = state;
  }

  auto format = format_;

  fmt::dynamic_format_arg_store<fmt::format_context> store;
  store.push_back(fmt::arg("mode", getFixModeString()));
  store.push_back(fmt::arg("status", getFixStatusString()));

  store.push_back(fmt::arg("latitude", gps_data_.fix.latitude));
  store.push_back(fmt::arg("latitude_error", gps_data_.fix.epy));

  store.push_back(fmt::arg("longitude", gps_data_.fix.longitude));
  store.push_back(fmt::arg("longitude_error", gps_data_.fix.epx));

  store.push_back(fmt::arg("altitude_hae", gps_data_.fix.altHAE));
  store.push_back(fmt::arg("altitude_msl", gps_data_.fix.altMSL));
  store.push_back(fmt::arg("altitude_error", gps_data_.fix.epv));

  store.push_back(fmt::arg("speed", gps_data_.fix.speed));
  store.push_back(fmt::arg("speed_error", gps_data_.fix.eps));

  store.push_back(fmt::arg("climb", gps_data_.fix.climb));
  store.push_back(fmt::arg("climb_error", gps_data_.fix.epc));

  store.push_back(fmt::arg("satellites_used", gps_data_.satellites_used));
  store.push_back(fmt::arg("satellites_visible", gps_data_.satellites_visible));

  auto text = fmt::vformat(format, store);

  if (tooltipEnabled()) {
    if (tooltip_format.empty() && config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    if (!tooltip_format.empty()) {
      auto tooltip_text = fmt::vformat(tooltip_format, store);
      if (label_.get_tooltip_text() != tooltip_text) {
        label_.set_tooltip_markup(tooltip_text);
      }
    } else if (label_.get_tooltip_text() != text) {
      label_.set_tooltip_markup(text);
    }
  }
  label_.set_markup(text);
  // Call parent update
  ALabel::update();
}

waybar::modules::Gps::~Gps() {
  gps_stream(&gps_data_, WATCH_DISABLE, NULL);
  gps_close(&gps_data_);
}
