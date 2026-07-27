#pragma once

#include <gdk/gdk.h>
#include <giomm/desktopappinfo.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/icon_loader.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {

// Plain data snapshot describing a single view (window), parsed off the sway
// IPC tree on the Ipc worker thread. Contains no GTK state.
struct TaskInfo {
  int64_t id = -1;
  std::string title;
  // Native Wayland views report an app_id; for XWayland views sway reports the
  // X11 instance and class hints instead. app_id holds the app_id (or the
  // instance), app_class the class, which is the better icon lookup key.
  std::string app_id;
  std::string app_class;
  bool active = false;
  bool fullscreen = false;
  bool urgent = false;
  std::string workspace;
  // Whether this window's workspace is the one currently displayed on its
  // output. Always true when "all-workspaces" is false, since the tree walk
  // filters non-visible workspaces out in that case.
  bool workspace_visible = false;
};

class Taskbar;

class Task {
 public:
  Task(const waybar::Bar&, const Json::Value&, Taskbar*, const TaskInfo&);
  ~Task();

  // made public so Taskbar can reorder based on configuration.
  Gtk::Button button;

  /* Getter functions */
  int64_t id() const { return id_; }
  std::string title() const { return title_; }
  std::string app_id() const { return app_id_; }
  bool active() const { return active_; }
  bool fullscreen() const { return fullscreen_; }
  bool urgent() const { return urgent_; }
  std::string workspace() const { return workspace_; }
  /* Whether the button is packed into the taskbar box. */
  bool visible() const { return button_visible_; }
  /* Whether the button is packed *and* actually rendered: with "active-only"
   * the buttons of inactive views stay packed but hidden. */
  bool shown() const { return button_visible_ && button.get_visible(); }

  void showButton();
  void hideButton();

  /* Update the cached snapshot for this task. Only ever called from the main
   * thread (from Taskbar::update()), so it is safe to touch GTK/IconTheme
   * here. */
  void setData(const TaskInfo&);

  /* Apply the ignored/squashed decision computed by Taskbar::update() (which
   * has the whole-snapshot view needed to keep exactly one instance of a
   * squashed group visible) and show or hide the button accordingly. */
  void applyVisibility(bool ignored, bool squashed);

  void update();

  /* Callbacks for Gtk events */
  bool handleClicked(GdkEventButton*);
  void handleDragDataGet(const Glib::RefPtr<Gdk::DragContext>& context,
                         Gtk::SelectionData& selection_data, guint info, guint time);
  void handleDragDataReceived(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
                              Gtk::SelectionData selection_data, guint info, guint time);

 private:
  std::string stateString(bool shortened = false) const;
  /* Expand one user-supplied format string. A malformed format (e.g. an
   * unknown {placeholder}) makes fmt throw; this warns once and yields an
   * empty string rather than letting the exception escape update(). */
  std::string formatText(const std::string& format, const std::string& title,
                         const std::string& name, const std::string& app_id);

  const Json::Value& config_;
  Taskbar* tbar_;

  int64_t id_;

  Gtk::Box content_;
  Gtk::Image icon_;
  Gtk::Label text_before_;
  Gtk::Label text_after_;
  Glib::RefPtr<Gio::DesktopAppInfo> app_info_;
  bool button_visible_ = false;

  bool with_icon_ = false;
  std::string format_before_;
  std::string format_after_;
  std::string format_tooltip_;
  bool format_warned_ = false;

  bool markup_ = false;
  bool active_only_ = false;
  int icon_size_ = 16;

  std::string name_;
  std::string title_;
  std::string app_id_;
  std::string app_class_;
  /* False until setData() has resolved app_info_/name_ once, so a task whose
   * app_id is empty still gets its initial resolution. */
  bool app_info_resolved_ = false;
  bool active_ = false;
  bool fullscreen_ = false;
  bool urgent_ = false;
  std::string workspace_;
  bool workspace_visible_ = false;
};

using TaskPtr = std::unique_ptr<Task>;

class Taskbar : public waybar::AModule {
 public:
  Taskbar(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Taskbar() override;
  void update() override;

  /* Used by Task to (un)pack and reorder its button. */
  void addButton(Gtk::Button&);
  void moveButton(Gtk::Button&, int);
  void removeButton(Gtk::Button&);
  /* Move the task with con_id `dragged_id` in front of `target_id` and persist
   * the resulting order. Unknown ids (e.g. a window that closed mid-drag) are
   * a no-op. */
  void reorderTask(int64_t dragged_id, int64_t target_id);

  Ipc& ipc();
  const IconLoader& iconLoader() const;

 private:
  void onEvent(const struct Ipc::ipc_response&);
  void onCmd(const struct Ipc::ipc_response&);

  bool allOutputs() const;
  bool allWorkspaces() const;
  void recordUserOrder();
  std::size_t taskAppIdCount(std::string_view app_id) const;
  std::size_t taskTitleCount(std::string_view title) const;
  void setBarCssClass(const std::string&, bool);

  const waybar::Bar& bar_;
  Gtk::Box box_;
  std::vector<TaskPtr> tasks_;

  IconLoader icon_loader_;
  std::unordered_set<std::string> ignore_list_;
  std::unordered_set<std::string> squash_list_;
  std::map<std::string, std::string> app_ids_replace_map_;

  bool bar_css_states_ = false;
  bool sort_by_app_id_ = false;
  bool active_first_ = false;
  bool homogeneous_ = false;
  bool expand_ = false;

  // Persisted drag-and-drop child order, keyed by sway con_id, applied on top
  // of the tree/sort order every update(). Ids of closed windows are kept until
  // the next drag rewrites the list; update() simply skips ids it cannot match,
  // so this is bounded by how many windows the user has ever dragged.
  std::vector<int64_t> user_order_;

  // Snapshot produced by onCmd() on the Ipc worker thread and consumed by
  // update() on the main thread. Guarded by mutex_.
  std::vector<TaskInfo> windows_;
  std::string current_workspace_;

  util::JsonParser parser_;
  std::mutex mutex_;
  // Must be declared last: its destructor joins the IPC worker thread, and the
  // worker posts to the widgets above, which must therefore outlive it.
  Ipc ipc_;
};

}  // namespace waybar::modules::sway
