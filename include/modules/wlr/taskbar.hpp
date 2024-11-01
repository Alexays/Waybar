#pragma once

#include <giomm/desktopappinfo.h>
#include <gtkmm/button.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include <unordered_set>

#include "bar.hpp"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

namespace waybar::modules::wlr {

struct widget_geometry {
  double x, y;
  int w, h;
};

class Taskbar;

class Task {
 public:
  Task(const waybar::Bar &, const Json::Value &, Taskbar *,
       struct zwlr_foreign_toplevel_handle_v1 *, struct wl_seat *);
  ~Task();

 public:
  enum State {
    MAXIMIZED = (1 << 0),
    MINIMIZED = (1 << 1),
    ACTIVE = (1 << 2),
    FULLSCREEN = (1 << 3),
    INVALID = (1 << 4)
  };
  // made public so TaskBar can reorder based on configuration.
  Glib::RefPtr<Gtk::Button> const button;
  struct widget_geometry minimize_hint;

 private:
  static uint32_t global_id;
  Glib::RefPtr<Gtk::GestureClick> const controllClick_;

  const waybar::Bar &bar_;
  const Json::Value &config_;
  Taskbar *tbar_;
  struct zwlr_foreign_toplevel_handle_v1 *handle_;
  struct wl_seat *seat_;

  uint32_t id_;

  Gtk::Box content_;
  Gtk::Image icon_;
  Gtk::Label text_before_;
  Gtk::Label text_after_;
  Glib::RefPtr<Gio::DesktopAppInfo> app_info_;
  bool button_visible_ = false;
  bool ignored_ = false;

  bool with_icon_ = false;
  bool with_name_ = false;
  std::string format_before_;
  std::string format_after_;

  std::string format_tooltip_;

  std::string name_;
  std::string title_;
  std::string app_id_;
  uint32_t state_ = 0;

  std::string repr() const;
  std::string state_string(bool = false) const;
  void set_minimize_hint();
  void set_app_info_from_app_id_list(const std::string &app_id_list);
  bool image_load_icon(Gtk::Image &image, const Glib::RefPtr<Gtk::IconTheme> &icon_theme,
                       Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size);
  void hide_if_ignored();

 public:
  /* Getter functions */
  uint32_t id() const { return id_; }
  std::string title() const { return title_; }
  std::string app_id() const { return app_id_; }
  uint32_t state() const { return state_; }
  bool maximized() const { return state_ & MAXIMIZED; }
  bool minimized() const { return state_ & MINIMIZED; }
  bool active() const { return state_ & ACTIVE; }
  bool fullscreen() const { return state_ & FULLSCREEN; }

  /* Callbacks for the wlr protocol */
  void handle_title(const char *);
  void handle_app_id(const char *);
  void handle_output_enter(struct wl_output *);
  void handle_output_leave(struct wl_output *);
  void handle_state(struct wl_array *);
  void handle_done();
  void handle_closed();

  /* Callbacks for Gtk events */
  void handleClick(int n_press, double dx, double dy);
  bool handleDropData(const Glib::ValueBase &, double, double);

  bool operator==(const Task &) const;
  bool operator!=(const Task &) const;

  void update();

  /* Interaction with the tasks */
  void maximize(bool);
  void minimize(bool);
  void activate();
  void fullscreen(bool);
  void close();
};

using TaskPtr = std::unique_ptr<Task>;

class Taskbar : public waybar::AModule {
 public:
  Taskbar(const std::string &, const waybar::Bar &, const Json::Value &);
  ~Taskbar();
  void update();
  Gtk::Widget &root() override;

 private:
  const waybar::Bar &bar_;
  Gtk::Box box_;
  std::vector<TaskPtr> tasks_;

  std::vector<Glib::RefPtr<Gtk::IconTheme>> icon_themes_;
  std::unordered_set<std::string> ignore_list_;
  std::map<std::string, std::string> app_ids_replace_map_;

  struct zwlr_foreign_toplevel_manager_v1 *manager_;
  struct wl_seat *seat_;

 public:
  /* Callbacks for global registration */
  void register_manager(struct wl_registry *, uint32_t name, uint32_t version);
  void register_seat(struct wl_registry *, uint32_t name, uint32_t version);

  /* Callbacks for the wlr protocol */
  void handle_toplevel_create(struct zwlr_foreign_toplevel_handle_v1 *);
  void handle_finished();

  void add_button(Glib::RefPtr<Gtk::Button>);
  void move_button(Glib::RefPtr<Gtk::Button>, int);
  void remove_button(Glib::RefPtr<Gtk::Button>);
  void remove_task(uint32_t);

  bool show_output(struct wl_output *) const;
  bool all_outputs() const;

  const std::vector<Glib::RefPtr<Gtk::IconTheme>> &icon_themes() const;
  const std::unordered_set<std::string> &ignore_list() const;
  const std::map<std::string, std::string> &app_ids_replace_map() const;
};

} /* namespace waybar::modules::wlr */
