#include "modules/hyprland/workbar/widget.hpp"
#include "modules/hyprland/workbar/workspace_button.hpp"

#include <iostream>
#include <memory>
#include <unordered_set>
#include <algorithm>

namespace waybar::modules::hyprland::workbar {

    Widget* Widget::instance_ = nullptr;

    Widget::Widget()
        : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {

        instance_ = this;
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

    Widget* Widget::instance() {
        return instance_;
    }

    WorkspaceButton* Widget::workspaceAt(int x, int y) {

    

    auto children = get_children();

    for (size_t i = 0; i < children.size(); ++i) {

        auto* button = dynamic_cast<WorkspaceButton*>(children[i]);

        if (!button) {
        
            continue;
        }

        int left, top;
        button->translate_coordinates(*this, 0, 0, left, top);


        int right;

        if (i + 1 < children.size()) {

            auto* next =
                dynamic_cast<WorkspaceButton*>(children[i + 1]);

            int next_left, next_top;
            next->translate_coordinates(*this, 0, 0, next_left, next_top);

            right = next_left;

        } else {

            right = get_allocation().get_width();
        }


        if (x >= left &&
            x < right) {

            return button;
        }
    }

    return nullptr;
}

void Widget::beginDrag(const WindowState& window) {

    dragging_ = true;
    dragged_window_ = window;
    hovered_workspace_ = nullptr;
}

void Widget::updateDrag(double x, double y) {

    static int count = 0;

    if (!dragging_) {

        return;
    }

    int wx, wy;
    get_window()->get_origin(wx, wy);

    auto* ws = workspaceAt(x - wx, y - wy);

    if (ws != hovered_workspace_) {

        hovered_workspace_ = ws;

    }
}

void Widget::endDrag() {

    if (!dragging_)
        return;

    dragging_ = false;

    if (hovered_workspace_) {

        std::string cmd =
            "hyprctl dispatch movetoworkspacesilent "
            + std::to_string(hovered_workspace_->id())
            + ",address:"
            + dragged_window_.address;

        std::system(cmd.c_str());
    }

    hovered_workspace_ = nullptr;
}

}  // namespace waybar::modules::hyprland