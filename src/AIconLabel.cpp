#include "AIconLabel.hpp"

#include <spdlog/spdlog.h>

namespace waybar {

AIconLabel::AIconLabel(const Json::Value &config, const std::string &name, const std::string &id,
                       const std::string &format, uint16_t interval, bool ellipsize,
                       bool enable_click, bool enable_scroll)
    : ALabel(config, name, id, format, interval, ellipsize, enable_click, enable_scroll) {
  box_.set_orientation(Gtk::Orientation::HORIZONTAL);

  int spacing = config_["icon-spacing"].isInt() ? config_["icon-spacing"].asInt() : 8;
  box_.set_spacing(spacing);
  spdlog::warn("Creating AIconLabel with {} children", get_children().size());

  box_.set_parent(*this);
  label_.unparent();  // necessary so we can add it to the box
  box_.append(image_);
  box_.append(label_);
}

auto AIconLabel::update() -> void {
  image_.set_visible(image_.get_visible() && iconEnabled());
  ALabel::update();
}

bool AIconLabel::iconEnabled() const {
  return config_["icon"].isBool() ? config_["icon"].asBool() : false;
}

Gtk::Widget const &AIconLabel::child() const { return box_; }

}  // namespace waybar
