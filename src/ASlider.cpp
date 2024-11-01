#include "ASlider.hpp"

#include "gtkmm/adjustment.h"

namespace waybar {

ASlider::ASlider(const Json::Value& config, const std::string& name, const std::string& id)
    : AModule(config, name, id, false, false),
      vertical_(config_["orientation"].asString() == "vertical"),
      scale_(vertical_ ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL) {
  scale_.set_name(name);
  if (!id.empty()) {
    scale_.get_style_context()->add_class(id);
  }
  scale_.get_style_context()->add_class(MODULE_CLASS);
  scale_.signal_value_changed().connect(sigc::mem_fun(*this, &ASlider::onValueChanged));

  if (config_["min"].isUInt()) {
    min_ = config_["min"].asUInt();
  }

  if (config_["max"].isUInt()) {
    max_ = config_["max"].asUInt();
  }

  scale_.set_inverted(vertical_);
  scale_.set_draw_value(false);
  scale_.set_adjustment(Gtk::Adjustment::create(curr_, min_, max_ + 1, 1, 1, 1));

  AModule::bindEvents(scale_);
}

void ASlider::onValueChanged() {}

Gtk::Widget& ASlider::root() { return scale_; };

}  // namespace waybar
