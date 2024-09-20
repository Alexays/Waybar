#include "modules/gps.hpp"

#include <cmath>

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
  constexpr const char *DEFAULT_FORMAT = "{status}";
}  // namespace


waybar::modules::Gps::Gps(const std::string& id, const Json::Value& config)
: ALabel(config, "gps", id, "{}", 30) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
  // TODO: connect to gpsd socket

  if (0 != gps_open("localhost", "2947", &gps_data_)) {
    throw std::runtime_error("Can't open gpsd socket");
  }

}

const int waybar::modules::Gps::getFixState() const {
0
}

const std::string waybar::modules::Gps::getFixStateName() const {
  switch (getFixState()) {
    case 1:
      return "fix-2d";
    case 2:
      return "fix-3d";
    default:
      return "fix-none";
  }
}

const std::string waybar::modules::Gps::getFixStateString() const {
  switch (getFixState()) {
    case 1:
      return "2D Fix";
    case 2:
      return "3D Fix";
    default:
      return "No fix";
  }
}

auto waybar::modules::Gps::update() -> void {

  std::string tooltip_format;

  if (!alt_) {
    auto state = getFixStateName();
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
  store.push_back(fmt::arg("status", getFixStateString()));

  label_.set_markup(fmt::vformat(format, store));
// Call parent update
ALabel::update();
}

waybar::modules::Gps::~Gps() {
  gps_close(&gps_data_);
}


