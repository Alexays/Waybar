#pragma once

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "modules/wayfire/backend.hpp"

namespace waybar::modules::wayfire {

class Window final : public AAppIconLabel {
 public:
  Window(const std::string& id, const Bar& bar, const Json::Value& config);
  ~Window() override;

 private:
  auto doUpdate() -> void override;
  auto update_icon_label() -> void;

  std::shared_ptr<IPC> ipc_;
  EventHandler handler_;
  const Bar& bar_;
  std::string old_app_id_;
};

}  // namespace waybar::modules::wayfire
