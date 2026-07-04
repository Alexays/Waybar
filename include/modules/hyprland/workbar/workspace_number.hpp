#pragma once

#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include "modules/hyprland/workbar/workspace_number.hpp"

namespace waybar::modules::hyprland::workbar {

class WorkspaceNumber : public Gtk::Button {
 public:
  explicit WorkspaceNumber(int workspace_id);
  int workspaceId() const;
  void setWorkspace(int workspace_id);
  Glib::RefPtr<Gtk::StyleContext> style() {
        return get_style_context();
    }

 protected:
    bool on_enter_notify_event(GdkEventCrossing* event) override;
    bool on_leave_notify_event(GdkEventCrossing* event) override;

 private:
  int workspace_id_;

  Gtk::Label label_;
};

}  // namespace waybar::modules::hyprland::workbar