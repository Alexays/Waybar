#include "ALabel.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <util/command.hpp>

#include "util/tooltip.hpp"

namespace waybar {

ALabel::ALabel(const Json::Value& config, const std::string& name, const std::string& id,
               const std::string& format, uint16_t interval, bool ellipsize, bool enable_click,
               bool enable_scroll)
    : AModule(config, name, id,
              config["format-alt"].isString() || config["menu"].isString() || enable_click,
              enable_scroll),
      format_(config_["format"].isString() ? config_["format"].asString() : format),

      // Leave the default option outside of the clamp, because the zero value
      // (default) is used in modules/custom.cpp to make the difference between
      // two types of custom scripts. Fixes #4521.
      interval_(
          config_["interval"] == "once"
              ? std::chrono::milliseconds::max()
              : std::chrono::milliseconds(
                    (config_["interval"].isNumeric()
                         ? (config_["interval"].asDouble() > 0
                                // 1ms precision; the cap keeps the cast and the nanosecond
                                // sleep from overflowing
                                ? static_cast<long>(
                                      std::clamp(config_["interval"].asDouble(), 0.001, 1e9) * 1000)
                                // Only modules with no periodic default use 0 as an
                                // event-driven sentinel. Periodic modules fall back to their
                                // default interval so interval:0 cannot busy-loop or hit
                                // modulo-by-zero clock code.
                                : (interval == 0 ? 0L : 1000L * static_cast<long>(interval)))
                         : 1000 * (long)interval))),
      default_format_(format_) {
  label_.set_name(name);
  if (!id.empty()) {
    label_.add_css_class(id);
  }
  label_.add_css_class(MODULE_CLASS);
  event_box_.set_child(label_);
  if (tooltipEnabled()) {
    // Keep dynamic tooltip contents out of GtkWidget's tooltip-markup property.
    // Setting that property queues a display-wide tooltip query, which restarts
    // GTK's hover delay when modules update faster than the delay can expire.
    label_.set_has_tooltip(true);
    label_.signal_query_tooltip().connect(
        [this](int, int, bool, const Glib::RefPtr<Gtk::Tooltip>& tooltip) {
          if (!last_tooltip_markup_.has_value() || last_tooltip_markup_->empty()) {
            active_tooltip_.reset();
            return false;
          }
          active_tooltip_ = tooltip;
          tooltip->set_markup(*last_tooltip_markup_);
          return true;
        },
        true);
    // GTK hides the tooltip on every scroll event and only shows it again once
    // the pointer moves, so a value being scrolled (volume, brightness) cannot
    // be read while changing it. The capture phase sees the scroll even when a
    // handler consumes it, but before GTK's own tooltip handling, hence the idle.
    tooltip_scroll_ = Gtk::EventControllerScroll::create();
    tooltip_scroll_->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
    tooltip_scroll_->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    tooltip_scroll_->signal_scroll().connect(
        [this](double, double) {
          tooltip_requery_.disconnect();
          tooltip_requery_ = Glib::signal_idle().connect([this] {
            if (pointerOverModule()) {
              label_.trigger_tooltip_query();
            }
            return false;
          });
          return false;
        },
        false);
    event_box_.add_controller(tooltip_scroll_);
  }
  if (config_["max-length"].isUInt()) {
    label_.set_max_width_chars(config_["max-length"].asInt());
    label_.set_ellipsize(Pango::EllipsizeMode::END);
    label_.set_single_line_mode(true);
  } else if (ellipsize && label_.get_max_width_chars() == -1) {
    label_.set_ellipsize(Pango::EllipsizeMode::END);
    label_.set_single_line_mode(true);
  }

  if (config_["min-length"].isUInt()) {
    label_.set_width_chars(config_["min-length"].asUInt());
  }

  if (config_["fit-height"].isBool()) {
    label_.setFitHeight(config_["fit-height"].asBool());
  }

  uint rotate = 0;

  if (config_["rotate"].isUInt()) {
    rotate = config["rotate"].asUInt();
    if (not(rotate == 0 || rotate == 90 || rotate == 180 || rotate == 270))
      spdlog::warn("'rotate' is only supported in 90 degree increments {} is not valid.", rotate);
    label_.setAngle(rotate);
  }

  if (config_["align"].isDouble()) {
    auto align = config_["align"].asFloat();
    if (rotate == 90 || rotate == 270) {
      label_.set_yalign(align);
    } else {
      label_.set_xalign(align);
    }
  }

  setupMenu();

  if (config_["justify"].isString()) {
    auto justify_str = config_["justify"].asString();
    if (justify_str == "left") {
      label_.set_justify(Gtk::Justification::LEFT);
    } else if (justify_str == "right") {
      label_.set_justify(Gtk::Justification::RIGHT);
    } else if (justify_str == "center") {
      label_.set_justify(Gtk::Justification::CENTER);
    }
  }
}

auto ALabel::update() -> void { AModule::update(); }

bool ALabel::setLabelMarkup(const Glib::ustring& markup) {
  if (last_label_markup_ == markup.raw()) {
    return false;
  }

  // GTK rejects markup that is not valid UTF-8 and keeps showing the previous text
  if (markup.validate()) {
    label_.set_markup(markup);
  } else {
    label_.set_markup(markup.make_valid());
  }
  last_label_markup_ = markup.raw();
  return true;
}

bool ALabel::pointerOverModule() const {
  return (event_box_.get_state_flags() & Gtk::StateFlags::PRELIGHT) == Gtk::StateFlags::PRELIGHT;
}

bool ALabel::setTooltipMarkup(const Glib::ustring& markup) {
  if (last_tooltip_markup_ == markup.raw()) {
    return false;
  }

  // Stored valid and bounded, since the query-tooltip handler hands it to GTK as well
  std::string tooltip = markup.validate() ? markup.raw() : markup.make_valid().raw();
  util::limit_tooltip_markup(tooltip);
  if (last_tooltip_markup_ == tooltip) {
    return false;
  }
  last_tooltip_markup_ = std::move(tooltip);
  if (active_tooltip_) {
    // GTK shares one tooltip per display: once the pointer left, it may show another module.
    if (pointerOverModule()) {
      active_tooltip_->set_markup(*last_tooltip_markup_);
    } else {
      active_tooltip_.reset();
    }
  }
  return true;
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
    if (size != 0U && format_icons[0].isObject()) {
      std::string last_icon;
      for (const auto& threshold : format_icons) {
        if (!threshold.isObject() || !threshold["icon"].isString() || !threshold["max"].isUInt()) {
          static bool warned = false;
          if (!warned) {
            spdlog::warn(
                "format-icons: skipping invalid threshold object, expected {\"icon\": \"...\", "
                "\"max\": N}");
            warned = true;
          }
          continue;
        }
        last_icon = threshold["icon"].asString();
        if (percentage <= threshold["max"].asUInt()) {
          return last_icon;
        }
      }
      if (!last_icon.empty()) {
        return last_icon;
      }
    } else if (size != 0U) {
      auto divisor = std::max(1U, (max == 0 ? 100U : static_cast<unsigned>(max)) / size);
      auto idx = std::clamp(percentage / divisor, 0U, size - 1);
      format_icons = format_icons[idx];
    }
  }
  if (format_icons.isString()) {
    return format_icons.asString();
  }
  return "";
}

std::string ALabel::getIcon(uint16_t percentage, const std::vector<std::string>& alts,
                            uint16_t max) {
  auto format_icons = config_["format-icons"];
  if (format_icons.isObject()) {
    std::string _alt = "default";
    for (const auto& alt : alts) {
      if (!alt.empty() && (format_icons[alt].isString() || format_icons[alt].isArray())) {
        _alt = alt;
        break;
      }
    }
    format_icons = format_icons[_alt];
  }
  if (format_icons.isArray()) {
    auto size = format_icons.size();
    if (size != 0U && format_icons[0].isObject()) {
      std::string last_icon;
      for (const auto& threshold : format_icons) {
        if (!threshold.isObject() || !threshold["icon"].isString() || !threshold["max"].isUInt()) {
          static bool warned = false;
          if (!warned) {
            spdlog::warn(
                "format-icons: skipping invalid threshold object, expected {\"icon\": \"...\", "
                "\"max\": N}");
            warned = true;
          }
          continue;
        }
        last_icon = threshold["icon"].asString();
        if (percentage <= threshold["max"].asUInt()) {
          return last_icon;
        }
      }
      if (!last_icon.empty()) {
        return last_icon;
      }
    } else if (size != 0U) {
      auto divisor = std::max(1U, (max == 0 ? 100U : static_cast<unsigned>(max)) / size);
      auto idx = std::clamp(percentage / divisor, 0U, size - 1);
      format_icons = format_icons[idx];
    }
  }
  if (format_icons.isString()) {
    return format_icons.asString();
  }
  return "";
}

void ALabel::copyToClipboard(const std::string& literal) {
  label_.get_clipboard()->set_text(literal);
}

bool waybar::ALabel::handleToggle(const ButtonEvent& e) {
  bool handled = false;
  if (config_["format-alt-click"].isUInt() && e.button == config_["format-alt-click"].asUInt()) {
    alt_ = !alt_;
    if (alt_ && config_["format-alt"].isString()) {
      format_ = config_["format-alt"].asString();
    } else {
      format_ = default_format_;
    }
    handled = true;
  }

  if (config_["on-click-copy"].isBool() && config_["on-click-copy"].asBool()) {
    copyToClipboard(label_.get_text());
    handled = true;
  }
  return AModule::handleToggle(e) || handled;
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
  std::ranges::sort(states.begin(), states.end(), [&lesser](auto& a, auto& b) {
    return lesser ? a.second < b.second : a.second > b.second;
  });
  std::string valid_state;
  for (auto const& state : states) {
    if ((lesser ? value <= state.second : value >= state.second) && valid_state.empty()) {
      label_.add_css_class(state.first);
      valid_state = state.first;
    } else {
      label_.remove_css_class(state.first);
    }
  }
  return valid_state;
}

}  // namespace waybar
