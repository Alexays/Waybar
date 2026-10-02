
#include "modules/hyprland/workbar/workspace_button.hpp"

#include <unordered_set>

namespace waybar::modules::hyprland::workbar {

int WorkspaceButton::id() const { return number_.workspaceId(); }

WorkspaceButton::WorkspaceButton(const WorkspaceState& workspace)
    : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL),
      number_(workspace.id),
      box_(Gtk::ORIENTATION_HORIZONTAL),
      icons_(Gtk::ORIENTATION_HORIZONTAL) {
  get_style_context()->add_class("workspace");

  box_.get_style_context()->add_class("workspace-content");

  icons_.get_style_context()->add_class("workspace-icons");
  number_.get_style_context()->add_class("workspace-number");

  box_.pack_start(number_, Gtk::PACK_SHRINK);
  box_.pack_start(icons_, Gtk::PACK_SHRINK);

  pack_start(box_, Gtk::PACK_EXPAND_WIDGET);

  setWorkspace(workspace);

  show_all();
}

void WorkspaceButton::setWorkspace(const WorkspaceState& workspace) {
  auto context = number_.style();

  std::unordered_set<std::string> seen;

  if (workspace.active) {
    context->add_class("active");
  } else {
    context->remove_class("active");
  }

  if (workspace.visible) {
    context->add_class("visible");
  } else {
    context->remove_class("visible");
  }

  for (const auto& window : workspace.windows) {
    seen.insert(window.address);

    auto it = window_icons_.find(window.address);

    if (it != window_icons_.end()) {
      it->second->setWindow(window);
      continue;
    }

    auto icon = std::make_unique<WindowIcon>(window);

    icons_.pack_start(*icon, Gtk::PACK_SHRINK);
    icon->show();

    window_icons_.emplace(window.address, std::move(icon));
  }

  for (auto it = window_icons_.begin(); it != window_icons_.end();) {
    if (!seen.contains(it->first)) {
      icons_.remove(*it->second);
      it = window_icons_.erase(it);
    } else {
      ++it;
    }
  }

  show();
}

}  // namespace waybar::modules::hyprland::workbar