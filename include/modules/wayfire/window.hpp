#pragma once

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "modules/wayfire/backend.hpp"

namespace waybar::modules::wayfire {

class Window : public AAppIconLabel {
  std::shared_ptr<IPC> ipc;
  EventHandler handler;

  const Bar& bar_;
  std::string old_app_id_;

 public:
  Window(const std::string& id, const Bar& bar, const Json::Value& config);
  ~Window() override;

  auto update() -> void override;
  auto update_icon_label() -> void;
};

}  // namespace waybar::modules::wayfire
