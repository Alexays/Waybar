#pragma once

#include <functional>
#include <string>

#include "modules/hyprland/backend.hpp"
#include "modules/hyprland/workbar/model.hpp"

namespace waybar::modules::hyprland::workbar {

class Backend : public EventHandler {
 public:
  Backend();

  WorkspaceList getWorkspaces();

  void onEvent(const std::string& ev) override;
  void setUpdateCallback(std::function<void()> callback);

 private:
  IPC& ipc_;

  std::function<void()> update_callback_;
};

}  // namespace waybar::modules::hyprland::workbar