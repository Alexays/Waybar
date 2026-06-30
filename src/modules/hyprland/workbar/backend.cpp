#include <iostream>
#include <unordered_set>
#include "modules/hyprland/workbar/backend.hpp"


namespace waybar::modules::hyprland::workbar {

Backend::Backend()
    : ipc_(IPC::inst()) {
    ipc_.registerForIPC("workspacev2", this);
    ipc_.registerForIPC("focusedmonv2", this);

    ipc_.registerForIPC("openwindow", this);
    ipc_.registerForIPC("closewindow", this);
    ipc_.registerForIPC("movewindowv2", this);
}

WorkspaceList Backend::getWorkspaces() {
    auto json = ipc_.getSocket1JsonReply("workspaces");
    auto clients = ipc_.getSocket1JsonReply("clients");

    WorkspaceList workspaces;

    auto monitors = ipc_.getSocket1JsonReply("monitors");

    std::unordered_set<int> active_workspaces;
    std::unordered_set<int> visible_workspaces;

    for (const auto& monitor : monitors) {
        int ws = monitor["activeWorkspace"]["id"].asInt();

        visible_workspaces.insert(ws);

        if (monitor["focused"].asBool()) {
            active_workspaces.insert(ws);
        }
    }

    for (const auto& ws : json) {
        WorkspaceState state;

        state.id = ws["id"].asInt();
        state.active = active_workspaces.contains(state.id);
        state.visible = visible_workspaces.contains(state.id);
        state.monitor = ws["monitor"].asString();

        for (const auto& client : clients) {
            if (client["workspace"]["id"].asInt() != state.id) {
                continue;
            }

            state.windows.push_back({
                client["address"].asString(),
                client["class"].asString(),
                client["title"].asString(),
            });
        }

        workspaces.push_back(std::move(state));
    }

    return workspaces;
}

void Backend::setUpdateCallback(std::function<void()> callback) {
  update_callback_ = std::move(callback);
}

void Backend::onEvent(const std::string& ev) {

    if (update_callback_) {
        update_callback_();
    }
}

}  // namespace waybar::modules::hyprland::workbar