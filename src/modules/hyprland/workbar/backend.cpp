#include <iostream>
#include "modules/hyprland/workbar/backend.hpp"


namespace waybar::modules::hyprland::workbar {

Backend::Backend()
    : ipc_(IPC::inst()) {
  ipc_.registerForIPC("workspacev2", this);
}

WorkspaceList Backend::getWorkspaces() {
    auto json = ipc_.getSocket1JsonReply("workspaces");

    WorkspaceList workspaces;

    for (const auto& ws : json) {
        workspaces.push_back({
            ws["id"].asInt()
        });
    }

    return workspaces;
}

void Backend::setUpdateCallback(std::function<void()> callback) {
  update_callback_ = std::move(callback);
}

void Backend::onEvent(const std::string&) {
  if (update_callback_) {
    update_callback_();
  }
}

}  // namespace waybar::modules::hyprland::workbar