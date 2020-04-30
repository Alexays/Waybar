#pragma once

#include "AModule.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "util/json.hpp"

#include <memory>
#include <string>
#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>

#include <wayland-client.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"


namespace waybar::modules::wlr {

class Taskbar;

class Task
{
   public:
    Task(const waybar::Bar&, Taskbar*, struct zwlr_foreign_toplevel_handle_v1 *);
    ~Task();

   public:
    enum State {
        MAXIMIZED = (1 << 0),
        MINIMIZED = (1 << 1),
        ACTIVE = (1 << 2),
        FULLSCREEN = (1 << 3),
        INVALID = (1 << 4)
    };

   private:
    static uint32_t global_id;

   private:
    const waybar::Bar &bar_;
    Taskbar *tbar_;
    struct zwlr_foreign_toplevel_handle_v1 *handle_;

    uint32_t id_;

    Gtk::Button button_;
    Gtk::Image icon_;
    bool button_visible_;

    std::string title_;
    std::string app_id_;
    uint32_t state_;

   private:
    std::string repr() const;

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

   public:
    /* Callbacks for the wlr protocol */
    void handle_title(const char *);
    void handle_app_id(const char *);
    void handle_output_enter(struct wl_output *);
    void handle_output_leave(struct wl_output *);
    void handle_state(struct wl_array *);
    void handle_done();
    void handle_closed();

  public:
    bool operator==(const Task&) const;
    bool operator!=(const Task&) const;

  public:
    /* Interaction with the tasks */
    void maximize(bool);
    void minimize(bool);
    void activate();
    void fullscreen(bool);
    void close();
};

using TaskPtr = std::unique_ptr<Task>;


class Taskbar : public waybar::AModule
{
   public:
    Taskbar(const std::string&, const waybar::Bar&, const Json::Value&);
    ~Taskbar();
    void update();

   private:
    const waybar::Bar &bar_;
    Gtk::Box box_;
    std::vector<TaskPtr> tasks_;

    struct zwlr_foreign_toplevel_manager_v1 *manager_;
    struct wlr_seat *seat_;

   public:
    /* Callbacks for global registration */
    void register_manager(struct wl_registry*, uint32_t name, uint32_t version);
    void register_seat(struct wl_registry*, uint32_t name, uint32_t version);

    /* Callbacks for the wlr protocol */
    void handle_toplevel_create(struct zwlr_foreign_toplevel_handle_v1 *);
    void handle_finished();

   public:
    void add_button(Gtk::Button &);
    void remove_button(Gtk::Button &);
    void remove_task(uint32_t);
};

} /* namespace waybar::modules::wlr */
