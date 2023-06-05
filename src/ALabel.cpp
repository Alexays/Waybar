#include "ALabel.hpp"

namespace waybar {

ALabel::ALabel(const Json::Value& config, const std::string& name, const std::string& id,
               const std::string& format, uint16_t interval, bool ellipsize, bool enable_click,
               bool enable_scroll)
    : AModule(config, name, id, format, config["format-alt"].isString() || enable_click,
              enable_scroll),
      interval_(config["interval"] == "once"
                    ? std::chrono::seconds(100000000)
                    : std::chrono::seconds(config["interval"].isUInt() ? config["interval"].asUInt()
                                                                       : interval)) {
  label_.set_name(name_);
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  event_box_.add(label_);
  if (config_["max-length"].isUInt()) {
    label_.set_max_width_chars(config_["max-length"].asInt());
    label_.set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
    label_.set_single_line_mode(true);
  } else if (ellipsize && label_.get_max_width_chars() == -1) {
    label_.set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
    label_.set_single_line_mode(true);
  }

  if (config_["min-length"].isUInt()) {
    label_.set_width_chars(config_["min-length"].asUInt());
  }

  uint rotate = 0;

  if (config_["rotate"].isUInt()) {
    rotate = config["rotate"].asUInt();
    label_.set_angle(rotate);
  }

  if (config_["align"].isDouble()) {
    auto align = config_["align"].asFloat();
    if (rotate == 90 || rotate == 270) {
      label_.set_yalign(align);
    } else {
      label_.set_xalign(align);
    }
  }
}

bool waybar::ALabel::handleToggle(GdkEventButton* const& e) {
  constexpr auto action{"format-alt-click"};
  if (config_[action].isUInt() && e->button == config_[action].asUInt()) {
    const auto dbusCli{dynamic_cast<DBusClient*>(this)};
    if (dbusCli)
      dbusCli->DBusClient::doAction(action);
    else
      Config::doAction(action);
  }
  return AModule::handleToggle(e);
}

std::string ALabel::getState(uint8_t value, bool lesser) {
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

}  // namespace waybar
