#include "modules/hyprland/workbar/widget.hpp"

#include <memory>
#include <unordered_set>
#include <algorithm>

namespace waybar::modules::hyprland::workbar {

    Widget::Widget()
        : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {

        show_all();
    }

    void Widget::clearWorkspaces() {
        for (auto& [id, button] : workspaces_) {
            remove(*button);
        }

        workspaces_.clear();
    }

    void Widget::setWorkspaces(const WorkspaceList& workspaces) {
        std::unordered_set<int> seen;

        for (const auto& ws : workspaces) {
            seen.insert(ws.id);

            auto it = workspaces_.find(ws.id);

            if (it != workspaces_.end()) {
                it->second->setWorkspace(ws);
                continue;
            }

            auto button = std::make_unique<WorkspaceButton>(ws);

            pack_start(*button, Gtk::PACK_SHRINK);

            workspaces_.emplace(ws.id, std::move(button));
        }

        for (auto it = workspaces_.begin(); it != workspaces_.end();) {
            if (!seen.contains(it->first)) {
                remove(*it->second);
                it = workspaces_.erase(it);
            } else {
                ++it;
            }
        }

        std::vector<int> ids;
        ids.reserve(workspaces_.size());

        for (const auto& [id, _] : workspaces_) {
            ids.push_back(id);
        }

        std::sort(ids.begin(), ids.end());

        for (int id : ids) {
            reorder_child(*workspaces_.at(id), id - 1);
        }

        show_all();
    }

}  // namespace waybar::modules::hyprland