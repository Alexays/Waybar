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

const std::string ALabel::getFormat() const { return format_; }

const std::vector<std::string> ALabel::getClasses() const { return {}; }

auto ALabel::update() -> void {
  // Remove all classes
  auto classes = label_.get_style_context()->list_classes();
  for (auto const& c : classes) {
    label_.get_style_context()->remove_class(c);
  }
  // Extract args output
  auto output = extractArgs(getFormat());
  label_.set_markup(output);
  if (label_.get_text().empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
  }
}

std::string ALabel::getIcon(uint16_t percentage, const std::string& alt, uint16_t max) const {
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
std::tuple<const Json::Value, const std::string> ALabel::handleArg(const std::string& format,
                                                                   const std::string& key,
                                                                   const Arg&         arg) const {
  bool def = key == "{}" || key.find("{:") != std::string::npos;
  if (def || format.find("{" + arg.key + "}") != std::string::npos ||
      format.find("{" + arg.key + ":") != std::string::npos) {
    auto val = arg.func();
    if (val.isString()) {
      auto varg = val.asString();
      return {val, def ? fmt::format(key, varg) : fmt::format(key, fmt::arg(arg.key, varg))};
    } else if (val.isInt()) {
      auto varg = val.asInt();
      return {val, def ? fmt::format(key, varg) : fmt::format(key, fmt::arg(arg.key, varg))};
    } else if (val.isUInt()) {
      auto varg = val.asUInt();
      return {val, def ? fmt::format(key, varg) : fmt::format(key, fmt::arg(arg.key, varg))};
    } else if (val.isDouble()) {
      auto varg = val.asDouble();
      return {val, def ? fmt::format(key, varg) : fmt::format(key, fmt::arg(arg.key, varg))};
    }
  }
  return {Json::Value{}, ""};
}

bool ALabel::checkFormatArg(const std::string& format, const Arg& arg) {
  return format.find("{" + arg.key + "}") != std::string::npos ||
         format.find("{" + arg.key + ":") != std::string::npos ||
         ((format.find("{}") != std::string::npos || format.find("{:") != std::string::npos) &&
          arg.state & DEFAULT);
}

const std::string ALabel::extractArgs(const std::string& format) {
  // TODO: workaround as fmt doest support dynamic named args
  std::vector<std::pair<std::string, std::string>> formats;
  std::vector<std::string>                         outputs;
  std::stringstream                                ss(format);
  std::string                                      tok;

  // Args requirement
  std::vector<std::tuple<Arg, Json::Value, std::string>> args;
  std::tuple<std::string, Json::Value, uint16_t>         state;

  while (getline(ss, tok, '}')) {
    if (tok.find('{') != std::string::npos) {
      auto str = tok + '}';
      auto it = std::find_if(args_.begin(), args_.end(), [this, &str](const auto& arg) {
        return checkFormatArg(str, arg);
      });
      formats.emplace_back(it != args_.end() ? it->key : "", tok + '}');
    } else {
      formats.emplace_back("", tok);
    }
  }

  for (const auto& arg : args_) {
    // Avoid useless logic
    if (arg.state & NONE || arg.state == DEFAULT || (arg.state == TOOLTIP && !tooltipEnabled())) {
      continue;
    }
    Json::Value val;
    // Check if the full format contains this arg
    if (!arg.key.empty() && checkFormatArg(format, arg)) {
      // Find the proper format
      auto it = std::find_if(formats.begin(), formats.end(), [&arg](const auto& form) {
        return form.first == arg.key;
      });
      if (it == formats.end()) {
        throw std::runtime_error("Can't find proper format: " + arg.key);
      }
      auto [value, output] = handleArg(format, it->second, arg);
      val = value;
      args.emplace_back(arg, value, output);
    } else {
      val = arg.func();
      args.emplace_back(arg, val, "");
    }
    if (arg.state & STATE || arg.state & REVERSED_STATE) {
      auto state_val = val.isConvertibleTo(Json::uintValue) ? val.asUInt() : 0;
      auto state_str = getState(state_val, arg.state & REVERSED_STATE);
      state_str = val.isString() && state_str.empty() ? val.asString() : state_str;
      state = {state_str, val, arg.state_threshold};
      if (!state_str.empty()) {
        label_.get_style_context()->add_class(state_str);
      }
    }
    auto classes = getClasses();
    for (const auto& c : classes) {
      label_.get_style_context()->add_class(c);
    }
    // If state arg is also tooltip
    if (arg.state & TOOLTIP && tooltipEnabled() && val.isConvertibleTo(Json::stringValue)) {
      label_.set_tooltip_text(val.asString());
    }
  }

  for (const auto& form : formats) {
    // Find proper arg
    if (!form.first.empty()) {
      auto it = std::find_if(args.begin(), args.end(), [&form](const auto& arg) {
        return std::get<0>(arg).key == form.first;
      });
      if (it != args.end()) {
        auto output = std::get<2>(*it);
        if (!output.empty()) {
          outputs.push_back(output);
        }
      }
    } else if (form.second.find("{icon}") != std::string::npos) {
      auto [state_str, state_val, state_max] = state;
      if (state_val.isNull() && state_str.empty()) {
        throw std::runtime_error("Icon arg need state arg");
      }
      auto percentage = state_val.isConvertibleTo(Json::uintValue) ? state_val.asUInt() : 0;
      auto icon =
          fmt::format(form.second, fmt::arg("icon", getIcon(percentage, state_str, state_max)));
      outputs.push_back(icon);
    } else {
      outputs.push_back(form.second);
    }
  }

  std::ostringstream oss;
  std::copy(outputs.begin(), outputs.end(), std::ostream_iterator<std::string>(oss, ""));
  return oss.str();
}

const std::string ALabel::getState(uint8_t value, bool lesser) const {
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
