#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <json/value.h>

#include <string>

namespace waybar::modules::niri {

class Workspaces;

class Workspace {
 public:
  Workspace(const Json::Value& workspace_data, Workspaces& manager);
  ~Workspace() = default;

  Workspace(const Workspace&) = delete;
  Workspace& operator=(const Workspace&) = delete;

  Gtk::Button& button() { return button_; }
  uint64_t id() const { return id_; }

  void update(const Json::Value& workspace_data, const std::vector<Json::Value>& all_windows);

 private:
  void rebuildTaskbar(const std::vector<Json::Value>& my_windows);

  Glib::RefPtr<Gdk::Pixbuf> loadIcon(const std::string& app_id, int size);

  Workspaces& manager_;
  uint64_t id_;

  // Layout:  button_
  //            └─ box_  (horizontal)
  //                 ├─ label_        workspace label / icon
  //                 └─ taskbar_box_  app icon buttons (shown only when taskbar enabled)
  Gtk::Button button_;
  Gtk::Box box_;   
  Gtk::Label label_;
  Gtk::Box taskbar_box_; 
};

}  // namespace waybar::modules::niri