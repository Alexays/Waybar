#include <iostream>
#include <unordered_set>
#include "modules/hyprland/workbar/backend.hpp"


namespace waybar::modules::hyprland::workbar {

Backend::Backend()
    : ipc_(IPC::inst()) {
  ipc_.registerForIPC("workspacev2", this);
}

WorkspaceList Backend::getWorkspaces() {
    auto json = ipc_.getSocket1JsonReply("workspaces");

    WorkspaceList workspaces;

    auto monitors = ipc_.getSocket1JsonReply("monitors");

    std::unordered_set<int> active_workspaces;

    for (const auto& monitor : monitors) {
    active_workspaces.insert(
        monitor["activeWorkspace"]["id"].asInt()
    );
}

    for (const auto& ws : json) {
        workspaces.push_back({
            .id = ws["id"].asInt(),
            .active = active_workspaces.contains(ws["id"].asInt()),
            .monitor = ws["monitor"].asString(),
            .windows = ws["windows"].asInt(),
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