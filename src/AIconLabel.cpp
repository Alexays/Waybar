#include "AIconLabel.hpp"

#include <gdkmm/pixbuf.h>
#include <spdlog/spdlog.h>

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
  if (not config_["swap-icon-label"].isBool())
    spdlog::warn("'swap-icon-label' must be a bool.");
  else
    swap_icon_label = config_["swap-icon-label"].asBool();

  if ((rot == 0 || rot == 3) ^ swap_icon_label) {
    box_.add(image_);
    box_.add(label_);
  } else {
    box_.add(label_);
    box_.add(image_);
  }

  event_box_.add(box_);
}

auto AIconLabel::update() -> void {
  image_.set_visible(image_.get_visible() && iconEnabled());
  ALabel::update();
}

bool AIconLabel::iconEnabled() const {
  return config_["icon"].isBool() ? config_["icon"].asBool() : false;
}

}  // namespace waybar
