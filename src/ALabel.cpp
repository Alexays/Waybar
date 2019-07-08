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
  if (format.find("{" + arg.first + "}") != std::string::npos ||
      format.find("{" + arg.first + ":") != std::string::npos) {
    auto val = arg.second.func();
    if (val.isString()) {
      auto varg = val.asString();
      return {val, fmt::format(key, fmt::arg(arg.first, varg))};
    } else if (val.isInt()) {
      auto varg = val.asInt();
      return {val, fmt::format(key, fmt::arg(arg.first, varg))};
    } else if (val.isUInt()) {
      auto varg = val.asUInt();
      return {val, fmt::format(key, fmt::arg(arg.first, varg))};
    } else if (val.isDouble()) {
      auto varg = val.asDouble();
      return {val, fmt::format(key, fmt::arg(arg.first, varg))};
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
  std::pair<std::string, uint8_t>                        state;

  while (getline(ss, tok, '}')) {
    if (tok.find('{') != std::string::npos) {
      auto it = std::find_if(args_.begin(), args_.end(), [&tok](const auto& arg) {
        return tok.find(arg.first) != std::string::npos;
      });
      if (it != args_.end()) {
        auto [val, output] = handleArg(format, tok + "}", *it);
        formats.push_back(output);
        if (it->second.isState && val.isConvertibleTo(Json::uintValue)) {
          auto stateVal = val.asUInt();
          state = {getState(stateVal, it->second.reversedState), stateVal};
        }
      } else if (tok.find("{icon}") != std::string::npos) {
        auto icon = fmt::format(tok + "}", fmt::arg(tok, getIcon(state.second, state.first)));
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
