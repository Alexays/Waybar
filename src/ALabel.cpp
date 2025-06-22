#include "ALabel.hpp"

#include <fmt/format.h>

#include <fstream>
#include <iostream>
#include <util/command.hpp>

#include "config.hpp"

namespace waybar {

ALabel::ALabel(const Json::Value& config, const std::string& name, const std::string& id,
               const std::string& format, uint16_t interval, bool ellipsize, bool enable_click,
               bool enable_scroll)
    : AModule(config, name, id,
              config["format-alt"].isString() || config["menu"].isString() || enable_click,
              enable_scroll),
      format_(config_["format"].isString() ? config_["format"].asString() : format),
      interval_(config_["interval"] == "once"
                    ? std::chrono::seconds::max()
                    : std::chrono::seconds(
                          config_["interval"].isUInt() ? config_["interval"].asUInt() : interval)),
      default_format_(format_) {
  label_.set_name(name);
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  label_.get_style_context()->add_class(MODULE_CLASS);
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
    if (not(rotate == 0 || rotate == 90 || rotate == 180 || rotate == 270))
      spdlog::warn("'rotate' is only supported in 90 degree increments {} is not valid.", rotate);
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

  // If a GTKMenu is requested in the config
  if (config_["menu"].isString()) {
    // Create the GTKMenu widget
    try {
      // Check that the file exists
      std::string menuFile = config_["menu-file"].asString();

      // there might be "~" or "$HOME" in original path, try to expand it.
      auto result = Config::tryExpandPath(menuFile, "");
      if (result.empty()) {
        throw std::runtime_error("Failed to expand file: " + menuFile);
      }

      menuFile = result.front();
      // Read the menu descriptor file
      std::ifstream file(menuFile);
      if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + menuFile);
      }
      std::stringstream fileContent;
      fileContent << file.rdbuf();
      GtkBuilder* builder = gtk_builder_new();

      // Make the GtkBuilder and check for errors in his parsing
      if (gtk_builder_add_from_string(builder, fileContent.str().c_str(), -1, nullptr) == 0U) {
        throw std::runtime_error("Error found in the file " + menuFile);
      }

      menu_ = gtk_builder_get_object(builder, "menu");
      if (menu_ == nullptr) {
        throw std::runtime_error("Failed to get 'menu' object from GtkBuilder");
      }
      submenus_ = std::map<std::string, GtkMenuItem*>();
      menuActionsMap_ = std::map<std::string, std::string>();

      // Linking actions to the GTKMenu based on
      for (Json::Value::const_iterator it = config_["menu-actions"].begin();
           it != config_["menu-actions"].end(); ++it) {
        std::string key = it.key().asString();
        submenus_[key] = GTK_MENU_ITEM(gtk_builder_get_object(builder, key.c_str()));
        menuActionsMap_[key] = it->asString();
        g_signal_connect(submenus_[key], "activate", G_CALLBACK(handleGtkMenuEvent),
                         (gpointer)menuActionsMap_[key].c_str());
      }
    } catch (std::runtime_error& e) {
      spdlog::warn("Error while creating the menu : {}. Menu popup not activated.", e.what());
    }
  }

  if (config_["justify"].isString()) {
    auto justify_str = config_["justify"].asString();
    if (justify_str == "left") {
      label_.set_justify(Gtk::Justification::JUSTIFY_LEFT);
    } else if (justify_str == "right") {
      label_.set_justify(Gtk::Justification::JUSTIFY_RIGHT);
    } else if (justify_str == "center") {
      label_.set_justify(Gtk::Justification::JUSTIFY_CENTER);
    }
  }
}

auto ALabel::update() -> void { AModule::update(); }

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
    if (size != 0U) {
      auto idx = std::clamp(percentage / ((max == 0 ? 100 : max) / size), 0U, size - 1);
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
    if (size != 0U) {
      auto idx = std::clamp(percentage / ((max == 0 ? 100 : max) / size), 0U, size - 1);
      format_icons = format_icons[idx];
    }
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

void ALabel::handleGtkMenuEvent(GtkMenuItem* /*menuitem*/, gpointer data) {
  waybar::util::command::res res = waybar::util::command::exec((char*)data, "GtkMenu");
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
