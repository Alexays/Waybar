#include "modules/hyprland/workbar/module.hpp"

namespace waybar::modules::hyprland::workbar {

Module::Module(const std::string& id, const Json::Value& config)
    : AModule(config, "workbar", id) {
  widget_.set_name("workbar");

  widget_.get_style_context()->add_class(MODULE_CLASS);

  if (!id.empty()) {
    widget_.get_style_context()->add_class(id);
  }

  event_box_.add(widget_);

  dp.emit();
}

void Module::update() {
  AModule::update();
}

}  // namespace waybar::modules::hyprland::workbar