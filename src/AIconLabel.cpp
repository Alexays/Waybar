#include "AIconLabel.hpp"

#include <gdkmm/pixbuf.h>
#include <spdlog/spdlog.h>
#include <regex>
#include <string>

namespace waybar {

AIconLabel::AIconLabel(const Json::Value &config, const std::string &name, const std::string &id,
                       const std::string &format, uint16_t interval, bool ellipsize,
                       bool enable_click, bool enable_scroll)
    : ALabel(config, name, id, format, interval, ellipsize, enable_click, enable_scroll) {
  event_box_.remove();
  label_.unset_name();
  label_.get_style_context()->remove_class(MODULE_CLASS);
  box_.get_style_context()->add_class(MODULE_CLASS);
  if (!id.empty()) {
    label_.get_style_context()->remove_class(id);
    box_.get_style_context()->add_class(id);
  }

  int rot = 0;

  if (config_["rotate"].isUInt()) {
    rot = config["rotate"].asUInt() % 360;
    if ((rot % 90) != 00) rot = 0;
    rot /= 90;
  }

  if ((rot % 2) == 0)
    box_.set_orientation(Gtk::Orientation::ORIENTATION_HORIZONTAL);
  else
    box_.set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);
  box_.set_name(name);

  int spacing = config_["icon-spacing"].isInt() ? config_["icon-spacing"].asInt() : 8;
  box_.set_spacing(spacing);

  bool swap_icon_label = false;
  if (config_["swap-icon-label"].isNull()) {
  } else if (config_["swap-icon-label"].isBool()) {
    swap_icon_label = config_["swap-icon-label"].asBool();
  } else {
    spdlog::warn("'swap-icon-label' must be a bool, found '{}'. Using default value (false).",
                 config_["swap-icon-label"].asString());
  }

  if ((rot == 0 || rot == 3) ^ swap_icon_label) {
    box_.add(image_);
    box_.add(label_);
  } else {
    box_.add(label_);
    box_.add(image_);
  }

  event_box_.add(box_);
}

std::tuple<std::string, std::string> AIconLabel::extractIcon(const std::string& input) {
  std::string iconResult = "";
  std::string labelResult = input;
  try {
    const std::regex iconSearch(R"((?=\\0icon\\1f).+?(?=\\n))");
    std::smatch iconMatch;
    if (std::regex_search(input, iconMatch, iconSearch)) {
      iconResult = iconMatch[0].str().substr(9);
    
      const std::regex cleanLabelPattern(R"(\\0icon\\1f.+?\\n)");
      labelResult = std::regex_replace(input, cleanLabelPattern, "");
    }
  } catch (const std::exception& e) {
      spdlog::warn("Error while parsing icon from label. {}", e.what());
  }

  return std::make_tuple(iconResult, labelResult);
}

auto AIconLabel::update() -> void {
  labelContainsIcon = false;

  auto [iconLabel, cleanLabel] = extractIcon(label_.get_label().c_str());
  labelContainsIcon = iconLabel.length() > 0;

  if (labelContainsIcon) {
    label_.set_markup(cleanLabel);

    if (iconLabel.front() == '/') {
      auto pixbuf = Gdk::Pixbuf::create_from_file(iconLabel);
      int scaled_icon_size = 6 * image_.get_scale_factor();
      pixbuf = Gdk::Pixbuf::create_from_file(iconLabel, scaled_icon_size, scaled_icon_size);

      auto surface = Gdk::Cairo::create_surface_from_pixbuf(pixbuf, image_.get_scale_factor(),
                                                            image_.get_window());
      image_.set(surface);
      image_.set_visible(true);
    } else {
      image_.set_from_icon_name(iconLabel, Gtk::ICON_SIZE_INVALID);
      image_.set_visible(true);
    }
  }

  image_.set_visible(image_.get_visible() && iconEnabled());
  ALabel::update();
}

bool AIconLabel::iconEnabled() const {
  return labelContainsIcon || (config_["icon"].isBool() ? config_["icon"].asBool() : false);
}

}  // namespace waybar
