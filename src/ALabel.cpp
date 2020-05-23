#include "ALabel.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <util/command.hpp>

namespace waybar {

ALabel::ALabel(const Json::Value& config,
               const std::string& name,
               const std::string& id,
               const std::string& format,
               const std::string& tooltipFormat,
               uint16_t interval,
               bool ellipsize)
    : AModule(config, name, id, tooltipFormat, config["format-alt"].isString()),
      interval_(config_["interval"] == "once"
                    ? std::chrono::seconds(100000000)
                    : std::chrono::seconds(
                          config_["interval"].isUInt() ? config_["interval"].asUInt() : interval)) {
  // Check interval type
  if (config_["interval"].isString()) {
    spdlog::warn("%s config has an incorrect interval type.", name);
  }

  label_.set_name(name);
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  event_box_.add(label_);

  if (config_["format"].isString()) {
    format_ = config_["format"].asString();
  } else {
    format_ = format;
  }

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
  auto text = label_.get_text();
  if (text.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
  }
  // Call parent update
  AModule::update();
}

auto ALabel::update(std::string format, fmt::dynamic_format_arg_store<fmt::format_context>& args)
    -> void {
  ALabel::update(format, args, "");
}

auto ALabel::update(std::string format,
                    fmt::dynamic_format_arg_store<fmt::format_context>& args,
                    std::string tooltipFormat) -> void {
  // Set the label
  if (alt_ && config_["format-alt"].isString()) {
    label_.set_markup(fmt::vformat(config_["format-alt"].asString(), args));
  } else {
    label_.set_markup(fmt::vformat(format, args));
  }

  // Hide the module on empty output
  auto text = label_.get_text();
  if (text.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();

    // Update tooltip only if label is not empty
    // Add tooltip if enabled and not already set
    if (AModule::tooltipEnabled()) {
      if (!tooltipFormat.empty()) {
        label_.set_tooltip_markup(fmt::vformat(tooltipFormat, args));
      } else if (config_["tooltip-format"].isString()) {
        auto tooltip_format = config_["tooltip-format"].asString();
        auto tooltip_text = fmt::vformat(tooltip_format, args);
        label_.set_tooltip_markup(tooltip_text);
      } else if (!tooltipFormat_.empty()) {
        label_.set_tooltip_markup(fmt::vformat(tooltipFormat_, args));
      }
    }
  }

  // Call parent update
  AModule::update();
}

bool ALabel::hasFormat(const std::string& key) const {
  return ALabel::hasFormat(key, format_);
}

bool ALabel::hasFormat(const std::string& key, const std::string& format) const {
  return format.find("{" + key + "}") != std::string::npos ||
         format.find("{" + key + ":") != std::string::npos;
}

const std::string& ALabel::getFormat() const { return format_; }

const std::string ALabel::getFormat(const std::string& prefix,
                                    const std::string& a,
                                    const std::string& b) {
  if (!b.empty() && config_[prefix + "-" + a + "-" + b].isString()) {
    return config_[prefix + "-" + a + "-" + b].asString();
  } else if (config_[prefix + "-" + a].isString()) {
    return config_[prefix + "-" + a].asString();
  } else if (!b.empty() && config_[prefix + "-" + b].isString()) {
    return config_[prefix + "-" + b].asString();
  }
  return "";
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
  }
  return AModule::handleToggle(e);
}

std::string ALabel::getState(uint16_t value, bool lesser) {
  if (!config_["states"].isObject()) {
    return "";
  }
  // Get current state
  std::vector<std::pair<std::string, uint16_t>> states;
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
