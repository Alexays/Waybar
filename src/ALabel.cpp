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
  auto [args, output] = extractArgs(format_);
  label_.set_markup(output);
  // TODO: getState
  if (args["usage"].isUInt()) {
    getState(args["usage"].asUInt());
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

std::tuple<Json::Value, const std::string> ALabel::extractArgs(const std::string& format) {
  // TODO: workaround as fmt doest support dynamic named args
  Json::Value              root(Json::objectValue);
  std::vector<std::string> formats;
  std::stringstream        ss(format);
  std::string              tok;

  while (getline(ss, tok, '}')) {
    if (tok.find('{') != std::string::npos) {
      for (const auto& arg : args_) {
        if (format.find("{" + arg.first + "}") != std::string::npos ||
            format.find("{" + arg.first + ":") != std::string::npos) {
          auto val = arg.second();
          if (val.isString()) {
            formats.push_back(fmt::format(tok + "}", fmt::arg(arg.first, val.asString())));
          } else if (val.isInt()) {
            formats.push_back(fmt::format(tok + "}", fmt::arg(arg.first, val.asInt())));
          } else if (val.isUInt()) {
            formats.push_back(fmt::format(tok + "}", fmt::arg(arg.first, val.asUInt())));
          } else if (val.isDouble()) {
            formats.push_back(fmt::format(tok + "}", fmt::arg(arg.first, val.asDouble())));
          }
        }
      }
    } else {
      formats.push_back(tok);
    }
  }
  std::ostringstream oss;
  std::copy(formats.begin(), formats.end(), std::ostream_iterator<std::string>(oss, ""));
  return {root, oss.str()};
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
