#include "modules/wlr/taskbar.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>

#include <spdlog/spdlog.h>


namespace waybar::modules::wlr {

uint32_t Task::global_id = 0;

static void tl_handle_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
        const char *title)
{
    return static_cast<Task*>(data)->handle_title(title);
}

static void tl_handle_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
        const char *app_id)
{
    return static_cast<Task*>(data)->handle_app_id(app_id);
}

static void tl_handle_output_enter(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
        struct wl_output *output)
{
    return static_cast<Task*>(data)->handle_output_enter(output);
}

static void tl_handle_output_leave(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
        struct wl_output *output)
{
    return static_cast<Task*>(data)->handle_output_leave(output);
}

static void tl_handle_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
        struct wl_array *state)
{
    return static_cast<Task*>(data)->handle_state(state);
}

static void tl_handle_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    return static_cast<Task*>(data)->handle_done();
}

static void tl_handle_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    return static_cast<Task*>(data)->handle_closed();
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_impl = {
    .title = tl_handle_title,
    .app_id = tl_handle_app_id,
    .output_enter = tl_handle_output_enter,
    .output_leave = tl_handle_output_leave,
    .state = tl_handle_state,
    .done = tl_handle_done,
    .closed = tl_handle_closed,
};

Task::Task(Taskbar* tbar, struct zwlr_foreign_toplevel_handle_v1 *tl_handle) :
    tbar_{tbar}, handle_{tl_handle}, id_{global_id++}
{
    zwlr_foreign_toplevel_handle_v1_add_listener(handle_, &toplevel_handle_impl, this);
}

Task::~Task()
{
    if (handle_) {
        zwlr_foreign_toplevel_handle_v1_destroy(handle_);
        handle_ = nullptr;
    }
}

std::string Task::repr() const
{
    std::stringstream ss;
    ss << "Task (" << id_ << ") " << title_ << " [" << app_id_ << "] "
        << (active() ? "A" : "a")
        << (maximized() ? "M" : "m")
        << (minimized() ? "I" : "i")
        << (fullscreen() ? "F" : "f");

    return ss.str();
}

void Task::handle_title(const char *title)
{
    title_ = title;
}

void Task::handle_app_id(const char *app_id)
{
    app_id_ = app_id;
}

void Task::handle_output_enter(struct wl_output *output)
{
    auto it = std::find(std::begin(outputs_), std::end(outputs_), output);
    if (it == std::end(outputs_)) {
        outputs_.push_back(output);
        spdlog::debug("{} entered output {}", repr(), (void*)output);
    }
}

void Task::handle_output_leave(struct wl_output *output)
{
    auto it = std::find(std::begin(outputs_), std::end(outputs_), output);
    if (it != std::end(outputs_)) {
        outputs_.erase(it);
        spdlog::debug("{} left output {}", repr(), (void*)output);
    }
}

void Task::handle_state(struct wl_array *state)
{
    state_ = 0;
    for (uint32_t* entry = static_cast<uint32_t*>(state->data);
         entry < static_cast<uint32_t*>(state->data) + state->size;
         entry++) {
        if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
            state_ |= MAXIMIZED;
        if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
            state_ |= MINIMIZED;
        if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            state_ |= ACTIVE;
        if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
            state_ |= FULLSCREEN;
    }
}

void Task::handle_done()
{
    spdlog::debug("{} changed", repr());
}

void Task::handle_closed()
{
    spdlog::debug("{} closed", repr());
    zwlr_foreign_toplevel_handle_v1_destroy(handle_);
    handle_ = nullptr;
    tbar_->remove_task(id_);
}

bool Task::operator==(const Task &o) const
{
    return o.id_ == id_;
}

bool Task::operator!=(const Task &o) const
{
    return o.id_ != id_;
}


static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
    spdlog::debug("taskbar: {} {} {}", name, interface, version);
    if (std::strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        static_cast<Taskbar*>(data)->register_manager(registry, name, version);
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        static_cast<Taskbar*>(data)->register_seat(registry, name, version);
    }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    /* Nothing to do here */
}

static const wl_registry_listener registry_listener_impl = {
    .global = handle_global,
    .global_remove = handle_global_remove
};

Taskbar::Taskbar(const std::string &id, const waybar::Bar &bar, const Json::Value &config) 
    : waybar::AModule(config, "taskbar", id, false, false),
      bar_(bar),
      box_{bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0},
      manager_{nullptr}, seat_{nullptr}
{
    struct wl_display *display = Client::inst()->wl_display;
    struct wl_registry *registry = wl_display_get_registry(display);

    wl_registry_add_listener(registry, &registry_listener_impl, this);
    //wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (!manager_) {
        spdlog::error("Failed to register as toplevel manager");
        return;
    }
    if (!seat_) {
        spdlog::error("Failed to get wayland seat");
        return;
    }
}

Taskbar::~Taskbar()
{
    if (manager_) {
        zwlr_foreign_toplevel_manager_v1_destroy(manager_);
        manager_ = nullptr;
    }
}

void Taskbar::update()
{
}

static void tm_handle_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager,
        struct zwlr_foreign_toplevel_handle_v1 *tl_handle)
{
    return static_cast<Taskbar*>(data)->handle_toplevel_create(tl_handle);
}

static void tm_handle_finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager)
{
    return static_cast<Taskbar*>(data)->handle_finished();
}

static const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_impl = {
    .toplevel = tm_handle_toplevel,
    .finished = tm_handle_finished,
};

void Taskbar::register_manager(struct wl_registry *registry, uint32_t name, uint32_t version)
{
    if (manager_) {
        spdlog::warn("Register foreign toplevel manager again although already existing!");
        return;
    }
    if (version != 2) {
        spdlog::warn("Using different foreign toplevel manager protocol version: {}", version);
    }

    manager_ = static_cast<struct zwlr_foreign_toplevel_manager_v1 *>(wl_registry_bind(registry, name,
            &zwlr_foreign_toplevel_manager_v1_interface, version));

    if (manager_)
        zwlr_foreign_toplevel_manager_v1_add_listener(manager_, &toplevel_manager_impl, this);
    else
        spdlog::debug("Failed to register manager");
}

void Taskbar::register_seat(struct wl_registry *registry, uint32_t name, uint32_t version)
{
    if (seat_) {
        spdlog::warn("Register seat again although already existing!");
        return;
    }

    seat_ = static_cast<struct wlr_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, version));
}

void Taskbar::handle_toplevel_create(struct zwlr_foreign_toplevel_handle_v1 *tl_handle)
{
    tasks_.push_back(std::make_unique<Task>(this, tl_handle));
}

void Taskbar::handle_finished()
{
    zwlr_foreign_toplevel_manager_v1_destroy(manager_);
    manager_ = nullptr;
}

void Taskbar::remove_task(uint32_t id)
{
    auto it = std::find_if(std::begin(tasks_), std::end(tasks_),
            [id](const TaskPtr &p) { return p->id() == id; });

    if (it == std::end(tasks_)) {
        spdlog::warn("Can't find task with id {}", id);
        return;
    }

    tasks_.erase(it);
}

} /* namespace waybar::modules::wlr */
