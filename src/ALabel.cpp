#include "ALabel.hpp"
#include <fmt/format.h>
#include <util/command.hpp>

#include <iostream>

namespace waybar {

ALabel::ALabel(const Json::Value& config, const std::string& name, const std::string& id,
               const std::string& format, uint16_t interval, bool ellipsize)
    : AModule(config, name, id, config["format-alt"].isString()),
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
  } else if (ellipsize && label_.get_max_width_chars() == -1) {
    label_.set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
  }

  if (config_["rotate"].isUInt()) {
    label_.set_angle(config["rotate"].asUInt());
  }
}

auto ALabel::update() -> void {
  auto output = extractArgs(format_);
  if (output.empty()) {
    event_box_.hide();
  } else {
    label_.set_markup(output);
    event_box_.show();
  }
}

std::string ALabel::getIcon(uint16_t percentage, const std::string& alt, uint16_t max) {
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

bool waybar::ALabel::handleToggle(GdkEventButton* const& e) {
  if (config_["format-alt-click"].isUInt() && e->button == config_["format-alt-click"].asUInt()) {
    alt_ = !alt_;
    if (alt_ && config_["format-alt"].isString()) {
      format_ = config_["format-alt"].asString();
    } else {
      format_ = default_format_;
    }
  }
  return AModule::handleToggle(e);
}

// Needed atm with the workaround and this json lib
std::tuple<Json::Value, const std::string> ALabel::handleArg(const std::string&          format,
                                                             const std::string&          key,
                                                             std::pair<std::string, Arg> arg) {
  bool def = key == "{}" || key.find("{:") != std::string::npos;
  if (def || format.find("{" + arg.first + "}") != std::string::npos ||
      format.find("{" + arg.first + ":") != std::string::npos) {
    auto val = arg.second.func();
    if (val.isString()) {
      auto varg = val.asString();
      return {val, def ? fmt::format(key, varg) : fmt::format(key, fmt::arg(arg.first, varg))};
    } else if (val.isInt()) {
      auto varg = val.asInt();
      return {val, def ? fmt::format(key, varg) : fmt::format(key, fmt::arg(arg.first, varg))};
    } else if (val.isUInt()) {
      auto varg = val.asUInt();
      return {val, def ? fmt::format(key, varg) : fmt::format(key, fmt::arg(arg.first, varg))};
    } else if (val.isDouble()) {
      auto varg = val.asDouble();
      return {val, def ? fmt::format(key, varg) : fmt::format(key, fmt::arg(arg.first, varg))};
    }
  }
  return {Json::Value(), ""};
}

const std::string ALabel::extractArgs(const std::string& format) {
  // TODO: workaround as fmt doest support dynamic named args
  std::vector<std::string> formats;
  std::stringstream        ss(format);
  std::string              tok;

  // Args requirement
  std::vector<std::tuple<Arg, Json::Value, std::string>> args;
  std::pair<std::string, Json::Value>                    state;

  // We need state arg first
  auto state_it =
      std::find_if(args_.begin(), args_.end(), [](const auto& arg) { return arg.second.isState; });
  if (state_it != args_.end()) {
    auto val = state_it->second.func();
    auto state_val = val.isConvertibleTo(Json::uintValue) ? val.asUInt() : 0;
    auto state_str = getState(state_val, state_it->second.reversedState);
    state_str = val.isString() && state_str.empty() ? val.asString() : state_str;
    state = {state_str, val};
    if (!old_state_.empty()) {
      label_.get_style_context()->remove_class(old_state_);
    }
    if (!state_str.empty()) {
      label_.get_style_context()->add_class(state_str);
      old_state_ = state_str;
    }
    // If state arg is also tooltip
    if (state_it->second.tooltip && tooltipEnabled() && val.isString()) {
      label_.set_tooltip_text(val.asString());
    }
  }

  while (getline(ss, tok, '}')) {
    if (tok.find('{') != std::string::npos) {
      auto str = tok + "}";
      auto it = std::find_if(args_.begin(), args_.end(), [&str](const auto& arg) {
        return ((str == "{}" || str.find("{:") != std::string::npos) && arg.second.isDefault) ||
               str.find(arg.first) != std::string::npos;
      });
      if (it != args_.end()) {
        auto [val, output] = handleArg(format, str, *it);
        formats.push_back(output);
        if (it->second.tooltip && tooltipEnabled() && val.isString()) {
          label_.set_tooltip_text(val.asString());
        }
      } else if (str.find("{icon}") != std::string::npos) {
        if (state.second.isNull() && state.first.empty()) {
          throw std::runtime_error("Icon arg need state arg.");
        }
        auto state_val = state.second.isConvertibleTo(Json::uintValue) ? state.second.asUInt() : 0;
        auto icon = fmt::format(str, fmt::arg("icon", getIcon(state_val, state.first)));
        formats.push_back(icon);
      }
    } else {
      formats.push_back(tok);
    }
  }
  std::ostringstream oss;
  std::copy(formats.begin(), formats.end(), std::ostream_iterator<std::string>(oss, ""));
  return oss.str();
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
