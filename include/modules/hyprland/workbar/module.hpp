#pragma once

#include "AModule.hpp"
#include "modules/hyprland/workbar/widget.hpp"

namespace waybar::modules::hyprland::workbar {

class Module : public AModule {
  public:
    Module(const std::string& id, const Json::Value& config);

    void update() override;

  private:
    Widget widget_;
};

}  // namespace waybar::modules::hyprland::workbar