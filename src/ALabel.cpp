#include "ALabel.hpp"

#include <giomm/simpleactiongroup.h>
#include <gtkmm/builder.h>
#include <spdlog/spdlog.h>

#include "config.hpp"
#include "util/command.hpp"

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
  set_name(name);
  if (!id.empty()) {
    add_css_class(id);
  }
  add_css_class(MODULE_CLASS);
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

  uint rotate = 0;
  if (config_["rotate"].isUInt()) {
    rotate = config["rotate"].asUInt();
    if (not(rotate == 0 || rotate == 90 || rotate == 180 || rotate == 270))
      spdlog::error("'rotate' is only supported in 90 degree increments {} is not valid.", rotate);
    // TODO
  }

  if (config_["align"].isDouble()) {
    auto align = config_["align"].asFloat();
    if (rotate == 90 || rotate == 270) {
      label_.set_yalign(align);
    } else {
      label_.set_xalign(align);
    }
  }

  if (config_["menu"].isString()) {
    // Create the menu
    try {
      // Check that the file exists
      std::string menuFile = config_["menu-file"].asString();

      // there might be "~" or "$HOME" in original path, try to expand it.
      auto result = Config::tryExpandPath(menuFile, "");
      if (result.empty()) {
        throw std::runtime_error("Failed to exapnd file: " + menuFile);
      }

      menuFile = result.front();
      auto builder = Gtk::Builder::create_from_file(menuFile);
      auto menuModel = builder->get_object<Gio::MenuModel>("menu");
      if (menuModel == nullptr) {
        throw std::runtime_error("Failed to get 'menu' object from GtkBuilder");
      }
      auto actionGroup = Gio::SimpleActionGroup::create();

      auto actions = config_["menu-actions"];
      for (Json::Value::const_iterator it = actions.begin(); it != actions.end(); ++it) {
        actionGroup->add_action(
            it.key().asString(),
            sigc::bind(sigc::mem_fun(*this, &ALabel::handleMenu), it->asString()));
      }
      menu_ = std::make_unique<Gtk::PopoverMenu>(menuModel, Gtk::PopoverMenu::Flags::NESTED);
      menu_->set_has_arrow(false);
      menu_->set_parent(*this);

      insert_action_group("menu", actionGroup);

      // Read the menu descriptor file
    } catch (const std::exception& e) {
      spdlog::warn("Error while creating the menu : {}. Menu popup not activated.", e.what());
    }
  }

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
  label_.set_parent(*this);

  AModule::bindEvents(*this);
}

ALabel::~ALabel() {
  if (!gobj()) {
    // If this was a managed widget, the underlying c object was already destroyed
    // NOTE: if a managed pointer is ever used for this, then we need to listen for signal_destroy
    // and unparent children in that case.
    return;
  }
  // Iterating over children means that subclasses can just use this destructor.
  while (Gtk::Widget* child = get_first_child()) {
    child->unparent();
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

void ALabel::setPopupPosition(Gtk::PositionType position) {
  if (menu_) {
    menu_->set_position(position);
  }
}

void waybar::ALabel::handleToggle(int n_press, double dx, double dy) {
  if (config_["format-alt-click"].isUInt() &&
      controllClick_->get_current_button() == config_["format-alt-click"].asUInt()) {
    alt_ = !alt_;
    if (alt_ && config_["format-alt"].isString()) {
      format_ = config_["format-alt"].asString();
    } else {
      format_ = default_format_;
    }
  }

  AModule::handleToggle(n_press, dx, dy);
}

void ALabel::handleMenu(std::string cmd) const { waybar::util::command::exec(cmd, "menu"); }

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
      add_css_class(state.first);
      valid_state = state.first;
    } else {
      remove_css_class(state.first);
    }
  }
  return valid_state;
}

void ALabel::handleClick(const std::string& name) {
  if (menu_ != nullptr && config_["menu"].asString() == name) {
    menu_->popup();
  }
}

Gtk::Widget const& ALabel::child() const { return label_; }
// this cast is safe because we are calling on a non-const this
Gtk::Widget& ALabel::child() {
  return const_cast<Gtk::Widget&>(const_cast<const ALabel*>(this)->child());
}

Gtk::SizeRequestMode ALabel::get_request_mode_vfunc() const { return child().get_request_mode(); }

void ALabel::measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
                           int& minimum_baseline, int& natural_baseline) const {
  return child().measure(orientation, for_size, minimum, natural, minimum_baseline,
                         natural_baseline);
}
void ALabel::size_allocate_vfunc(int width, int height, int baseline) {
  Gtk::Allocation allocation;
  allocation.set_x(0);
  allocation.set_y(0);
  allocation.set_width(width);
  allocation.set_height(height);

  child().size_allocate(allocation, baseline);
  if (menu_) {
    // We need to present the menu as part of this function
    menu_->present();
  }
}

Gtk::Widget& ALabel::root() { return *this; };
}  // namespace waybar
