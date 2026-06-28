#pragma once

#include "AModule.hpp"
#include "modules/hyprland/workbar_widget.hpp"

namespace waybar::modules::hyprland {

class Workbar : public AModule {
 public:
  Workbar(const std::string& id, const Json::Value& config);

  void update() override;

 private:
  WorkbarWidget widget_;
};

}  // namespace waybar::modules::hyprland