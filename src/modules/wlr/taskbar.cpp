#include "modules/wlr/taskbar.hpp"

#include "glibmm/error.h"
#include "glibmm/fileutils.h"
#include "glibmm/refptr.h"
#include "util/format.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

#include <gdkmm/monitor.h>

#include <gtkmm/icontheme.h>

#include <giomm/desktopappinfo.h>
#include <gio/gdesktopappinfo.h>

#include <spdlog/spdlog.h>


namespace waybar::modules::wlr {

/* String manipulation methods */
const std::string WHITESPACE = " \n\r\t\f\v";

static std::string ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

static std::string rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

static std::string trim(const std::string& s)
{
    return rtrim(ltrim(s));
}


/* Icon loading functions */
static std::vector<std::string> search_prefix()
{
    std::vector<std::string> prefixes = {""};

    auto xdg_data_dirs = std::getenv("XDG_DATA_DIRS");
    if (!xdg_data_dirs) {
        prefixes.emplace_back("/usr/share/");
        prefixes.emplace_back("/usr/local/share/");
    } else {
        std::string xdg_data_dirs_str(xdg_data_dirs);
        size_t start = 0, end = 0;

        do {
            end = xdg_data_dirs_str.find(':', start);
            auto p = xdg_data_dirs_str.substr(start, end-start);
            prefixes.push_back(trim(p) + "/");

            start = end == std::string::npos ? end : end + 1;
        } while(end != std::string::npos);
    }

    std::string home_dir = std::getenv("HOME");
    prefixes.push_back(home_dir + "/.local/share/");

    for (auto& p : prefixes)
        spdlog::debug("Using 'desktop' search path prefix: {}", p);

    return prefixes;
}

static Glib::RefPtr<Gdk::Pixbuf> load_icon_from_file(std::string icon_path, int size)
{
    try {
        auto pb = Gdk::Pixbuf::create_from_file(icon_path, size, size);
        return pb;
    } catch(...) {
        return {};
    }
}

/* Method 1 - get the correct icon name from the desktop file */
static std::string get_from_desktop_app_info(const std::string &app_id)
{
    static std::vector<std::string> prefixes = search_prefix();

    std::vector<std::string> app_folders = {
        "",
        "applications/",
        "applications/kde/",
        "applications/org.kde."
    };

    std::vector<std::string> suffixes = {
        "",
        ".desktop"
    };

    Glib::RefPtr<Gio::DesktopAppInfo> app_info;

    for (auto& prefix : prefixes)
        for (auto& folder : app_folders)
            for (auto& suffix : suffixes)
                if (!app_info)
                    app_info = Gio::DesktopAppInfo::create_from_filename(prefix + folder + app_id + suffix);

    if (app_info && app_info->get_icon())
        return app_info->get_icon()->to_string();

    return "";
}

/* Method 2 - use the app_id and check whether there is an icon with this name in the icon theme */
static std::string get_from_icon_theme(const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
        const std::string &app_id)
{
    if (icon_theme->lookup_icon(app_id, 24))
        return app_id;

    return "";
}

/* Method 3 - as last resort perform a search for most appropriate desktop info file */
static std::string get_from_desktop_app_info_search(const std::string &app_id)
{
    std::string desktop_file = "";

    gchar*** desktop_list = g_desktop_app_info_search(app_id.c_str());
    if (desktop_list != nullptr && desktop_list[0] != nullptr) {
        for (size_t i=0; desktop_list[0][i]; i++) {
            if (desktop_file == "") {
                desktop_file = desktop_list[0][i];
            } else {
                auto tmp_info = Gio::DesktopAppInfo::create(desktop_list[0][i]);
                auto startup_class = tmp_info->get_startup_wm_class();

                if (startup_class == app_id) {
                    desktop_file = desktop_list[0][i];
                    break;
                }
            }
        }
        g_strfreev(desktop_list[0]);
    }
    g_free(desktop_list);

    return get_from_desktop_app_info(desktop_file);
}

static bool image_load_icon(Gtk::Image& image, const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
        const std::string &app_id_list, int size)
{
    std::string app_id;
    std::istringstream stream(app_id_list);
    bool found = false;

    /* Wayfire sends a list of app-id's in space separated format, other compositors
     * send a single app-id, but in any case this works fine */
    while (stream >> app_id)
    {
        size_t start = 0, end = app_id.size();
        start = app_id.rfind(".", end);
        std::string app_name = app_id.substr(start+1, app_id.size());

        auto lower_app_id = app_id;
        std::transform(lower_app_id.begin(), lower_app_id.end(), lower_app_id.begin(),
                [](char c){ return std::tolower(c); });

        std::string icon_name = get_from_icon_theme(icon_theme, app_id);

        if (icon_name.empty())
            icon_name = get_from_icon_theme(icon_theme, lower_app_id);
        if (icon_name.empty())
            icon_name = get_from_icon_theme(icon_theme, app_name);
        if (icon_name.empty())
            icon_name = get_from_desktop_app_info(app_id);
        if (icon_name.empty())
            icon_name = get_from_desktop_app_info(lower_app_id);
        if (icon_name.empty())
            icon_name = get_from_desktop_app_info(app_name);
        if (icon_name.empty())
            icon_name = get_from_desktop_app_info_search(app_id);

        if (icon_name.empty())
            icon_name = "unknown";

        Glib::RefPtr<Gdk::Pixbuf> pixbuf;

        try {
            pixbuf = icon_theme->load_icon(icon_name, size, Gtk::ICON_LOOKUP_FORCE_SIZE);
        } catch(...) {
            if (Glib::file_test(icon_name, Glib::FILE_TEST_EXISTS))
                pixbuf = load_icon_from_file(icon_name, size);
            else
                pixbuf = {};
        }

        if (pixbuf) {
            image.set(pixbuf);
            found = true;
            break;
        }
    }

    return found;
}

/* Task class implementation */
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

static void tl_handle_parent(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
        struct zwlr_foreign_toplevel_handle_v1 *parent)
{
    /* This is explicitly left blank */
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
    .parent = tl_handle_parent,
};

Task::Task(const waybar::Bar &bar, const Json::Value &config, Taskbar *tbar,
        struct zwlr_foreign_toplevel_handle_v1 *tl_handle, struct wl_seat *seat) :
    bar_{bar}, config_{config}, tbar_{tbar}, handle_{tl_handle}, seat_{seat},
    id_{global_id++},
    content_{bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0},
    button_visible_{false}
{
    zwlr_foreign_toplevel_handle_v1_add_listener(handle_, &toplevel_handle_impl, this);

    button_.set_relief(Gtk::RELIEF_NONE);

    content_.add(text_before_);
    content_.add(icon_);
    content_.add(text_after_);

    content_.show();
    button_.add(content_);

    with_icon_ = false;
    format_before_.clear();
    format_after_.clear();

    if (config_["format"].isString()) {
        /* The user defined a format string, use it */
        auto format = config_["format"].asString();

        auto icon_pos = format.find("{icon}");
        if (icon_pos == 0) {
            with_icon_ = true;
            format_after_ = format.substr(6);
        } else if (icon_pos == std::string::npos) {
            format_before_ = format;
        } else {
            with_icon_ = true;
            format_before_ = format.substr(0, icon_pos);
            format_after_ = format.substr(icon_pos + 6);
        }
    } else {
        /* The default is to only show the icon */
        with_icon_ = true;
    }

    /* Strip spaces at the beginning and end of the format strings */
    format_tooltip_.clear();
    if (!config_["tooltip"].isBool() || config_["tooltip"].asBool()) {
        if (config_["tooltip-format"].isString())
            format_tooltip_ = config_["tooltip-format"].asString();
        else
            format_tooltip_ = "{title}";
    }

    /* Handle click events if configured */
    if (config_["on-click"].isString() || config_["on-click-middle"].isString()
            || config_["on-click-right"].isString()) {
        button_.add_events(Gdk::BUTTON_PRESS_MASK);
        button_.signal_button_press_event().connect(
                sigc::mem_fun(*this, &Task::handle_clicked), false);
    }
}

Task::~Task()
{
    if (handle_) {
        zwlr_foreign_toplevel_handle_v1_destroy(handle_);
        handle_ = nullptr;
    }
    if (button_visible_) {
        tbar_->remove_button(button_);
        button_visible_ = false;
    }
}

std::string Task::repr() const
{
    std::stringstream ss;
    ss << "Task (" << id_ << ") " << title_ << " [" << app_id_ << "] <"
       << (active() ? "A" : "a")
       << (maximized() ? "M" : "m")
       << (minimized() ? "I" : "i")
       << (fullscreen() ? "F" : "f")
       << ">";

    return ss.str();
}

std::string Task::state_string(bool shortened) const
{
    std::stringstream ss;
    if (shortened)
        ss << (minimized() ? "m" : "") << (maximized() ? "M" : "")
           << (active() ? "A" : "") << (fullscreen() ? "F" : "");
    else
        ss << (minimized() ? "minimized " : "") << (maximized() ? "maximized " : "")
           << (active() ? "active " : "") << (fullscreen() ? "fullscreen " : "");

    std::string res = ss.str();
    if (shortened || res.empty())
        return res;
    else
        return res.substr(0, res.size() - 1);
}

void Task::handle_title(const char *title)
{
    title_ = title;
}

void Task::handle_app_id(const char *app_id)
{
    app_id_ = app_id;

    if (!with_icon_)
        return;

    int icon_size = config_["icon-size"].isInt() ? config_["icon-size"].asInt() : 16;
    bool found = false;
    for (auto& icon_theme : tbar_->icon_themes()) {
        if (image_load_icon(icon_, icon_theme, app_id_, icon_size)) {
            found = true;
            break;
        }
    }

    if (found)
        icon_.show();
    else
        spdlog::debug("Couldn't find icon for {}", app_id_);
}

void Task::handle_output_enter(struct wl_output *output)
{
    spdlog::debug("{} entered output {}", repr(), (void*)output);

    if (!button_visible_ && (tbar_->all_outputs() || tbar_->show_output(output))) {
        /* The task entered the output of the current bar make the button visible */
        tbar_->add_button(button_);
        button_.show();
        button_visible_ = true;
        spdlog::debug("{} now visible on {}", repr(), bar_.output->name);
    }
}

void Task::handle_output_leave(struct wl_output *output)
{
    spdlog::debug("{} left output {}", repr(), (void*)output);

    if (button_visible_ && !tbar_->all_outputs() && tbar_->show_output(output)) {
        /* The task left the output of the current bar, make the button invisible */
        tbar_->remove_button(button_);
        button_.hide();
        button_visible_ = false;
        spdlog::debug("{} now invisible on {}", repr(), bar_.output->name);
    }
}

void Task::handle_state(struct wl_array *state)
{
    state_ = 0;
    size_t size = state->size / sizeof(uint32_t);
    for (size_t i = 0; i < size; ++i) {
        auto entry = static_cast<uint32_t*>(state->data)[i];
        if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
            state_ |= MAXIMIZED;
        if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
            state_ |= MINIMIZED;
        if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            state_ |= ACTIVE;
        if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
            state_ |= FULLSCREEN;
    }
}

void Task::handle_done()
{
    spdlog::debug("{} changed", repr());

    if (state_ & MAXIMIZED) {
        button_.get_style_context()->add_class("maximized");
    } else if (!(state_ & MAXIMIZED)) {
        button_.get_style_context()->remove_class("maximized");
    }

    if (state_ & MINIMIZED) {
        button_.get_style_context()->add_class("minimized");
    } else if (!(state_ & MINIMIZED)) {
        button_.get_style_context()->remove_class("minimized");
    }

    if (state_ & ACTIVE) {
        button_.get_style_context()->add_class("active");
    } else if (!(state_ & ACTIVE)) {
        button_.get_style_context()->remove_class("active");
    }

    if (state_ & FULLSCREEN) {
        button_.get_style_context()->add_class("fullscreen");
    } else if (!(state_ & FULLSCREEN)) {
        button_.get_style_context()->remove_class("fullscreen");
    }

    if (config_["active-first"].isBool() && config_["active-first"].asBool() && active())
        tbar_->move_button(button_, 0);

    tbar_->dp.emit();
}

void Task::handle_closed()
{
    spdlog::debug("{} closed", repr());
    zwlr_foreign_toplevel_handle_v1_destroy(handle_);
    handle_ = nullptr;
    if (button_visible_) {
        tbar_->remove_button(button_);
        button_visible_ = false;
    }
    tbar_->remove_task(id_);
}

bool Task::handle_clicked(GdkEventButton *bt)
{
    std::string action;
    if (config_["on-click"].isString() && bt->button == 1)
        action = config_["on-click"].asString();
    else if (config_["on-click-middle"].isString() && bt->button == 2)
        action = config_["on-click-middle"].asString();
    else if (config_["on-click-right"].isString() && bt->button == 3)
        action = config_["on-click-right"].asString();

    if (action.empty())
        return true;
    else if (action == "activate")
        activate();
    else if (action == "minimize")
        minimize(!minimized());
    else if (action == "minimize-raise"){
        if (minimized())
            minimize(false);
        else if (active())
            minimize(true);
        else
            activate();
    }
    else if (action == "maximize")
        maximize(!maximized());
    else if (action == "fullscreen")
        fullscreen(!fullscreen());
    else if (action == "close")
        close();
    else
        spdlog::warn("Unknown action {}", action);

    return true;
}

bool Task::operator==(const Task &o) const
{
    return o.id_ == id_;
}

bool Task::operator!=(const Task &o) const
{
    return o.id_ != id_;
}

void Task::update()
{
    bool markup = config_["markup"].isBool() ? config_["markup"].asBool() : false;
    std::string title = title_;
    std::string app_id = app_id_;
    if (markup) {
        title = Glib::Markup::escape_text(title);
        app_id = Glib::Markup::escape_text(app_id);
    }
    if (!format_before_.empty()) {
        auto txt = fmt::format(format_before_,
                    fmt::arg("title", title),
                    fmt::arg("app_id", app_id),
                    fmt::arg("state", state_string()),
                    fmt::arg("short_state", state_string(true))
                );
        if (markup)
            text_before_.set_markup(txt);
        else
            text_before_.set_label(txt);
        text_before_.show();
    }
    if (!format_after_.empty()) {
        auto txt = fmt::format(format_after_,
                    fmt::arg("title", title),
                    fmt::arg("app_id", app_id),
                    fmt::arg("state", state_string()),
                    fmt::arg("short_state", state_string(true))
                );
        if (markup)
            text_after_.set_markup(txt);
        else
            text_after_.set_label(txt);
        text_after_.show();
    }

    if (!format_tooltip_.empty()) {
        auto txt = fmt::format(format_tooltip_,
                    fmt::arg("title", title),
                    fmt::arg("app_id", app_id),
                    fmt::arg("state", state_string()),
                    fmt::arg("short_state", state_string(true))
                );
        if (markup)
            button_.set_tooltip_markup(txt);
        else
            button_.set_tooltip_text(txt);
    }
}

void Task::maximize(bool set)
{
    if (set)
        zwlr_foreign_toplevel_handle_v1_set_maximized(handle_);
    else
        zwlr_foreign_toplevel_handle_v1_unset_maximized(handle_);
}

void Task::minimize(bool set)
{
    if (set)
        zwlr_foreign_toplevel_handle_v1_set_minimized(handle_);
    else
        zwlr_foreign_toplevel_handle_v1_unset_minimized(handle_);
}

void Task::activate()
{
    zwlr_foreign_toplevel_handle_v1_activate(handle_, seat_);
}

void Task::fullscreen(bool set)
{
    if (zwlr_foreign_toplevel_handle_v1_get_version(handle_) < ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_SET_FULLSCREEN_SINCE_VERSION) {
        spdlog::warn("Foreign toplevel manager server does not support for set/unset fullscreen.");
        return;
    }

    if (set)
        zwlr_foreign_toplevel_handle_v1_set_fullscreen(handle_, nullptr);
    else
        zwlr_foreign_toplevel_handle_v1_unset_fullscreen(handle_);
}

void Task::close()
{
    zwlr_foreign_toplevel_handle_v1_close(handle_);
}


/* Taskbar class implementation */
static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
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
    box_.set_name("taskbar");
    if (!id.empty()) {
        box_.get_style_context()->add_class(id);
    }
    event_box_.add(box_);

    struct wl_display *display = Client::inst()->wl_display;
    struct wl_registry *registry = wl_display_get_registry(display);

    wl_registry_add_listener(registry, &registry_listener_impl, this);
    wl_display_roundtrip(display);

    if (!manager_) {
        spdlog::error("Failed to register as toplevel manager");
        return;
    }
    if (!seat_) {
        spdlog::error("Failed to get wayland seat");
        return;
    }

    /* Get the configured icon theme if specified */
    if (config_["icon-theme"].isArray()) {
        for (auto& c : config_["icon-theme"]) {
            auto it_name = c.asString();

            auto it = Gtk::IconTheme::create();
            it->set_custom_theme(it_name);
            spdlog::debug("Use custom icon theme: {}", it_name);

            icon_themes_.push_back(it);
        }
    } else if (config_["icon-theme"].isString()) {
        auto it_name = config_["icon-theme"].asString();

        auto it = Gtk::IconTheme::create();
        it->set_custom_theme(it_name);
        spdlog::debug("Use custom icon theme: {}", it_name);

        icon_themes_.push_back(it);
    }
    icon_themes_.push_back(Gtk::IconTheme::get_default());
}

Taskbar::~Taskbar()
{
    if (manager_) {
        struct wl_display *display = Client::inst()->wl_display;
        /*
         * Send `stop` request and wait for one roundtrip.
         * This is not quite correct as the protocol encourages us to wait for the .finished event,
         * but it should work with wlroots foreign toplevel manager implementation.
         */
        zwlr_foreign_toplevel_manager_v1_stop(manager_);
        wl_display_roundtrip(display);

        if (manager_) {
            spdlog::warn("Foreign toplevel manager destroyed before .finished event");
            zwlr_foreign_toplevel_manager_v1_destroy(manager_);
            manager_ = nullptr;
        }
    }
}

void Taskbar::update()
{
    for (auto& t : tasks_) {
        t->update();
    }

    AModule::update();
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
    if (version < ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_SET_FULLSCREEN_SINCE_VERSION) {
        spdlog::warn("Foreign toplevel manager server does not have the appropriate version."
                " To be able to use all features, you need at least version 2, but server is version {}", version);
    }

    // limit version to a highest supported by the client protocol file
    version = std::min<uint32_t>(version, zwlr_foreign_toplevel_manager_v1_interface.version);

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
    version = std::min<uint32_t>(version, wl_seat_interface.version);

    seat_ = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, version));
}

void Taskbar::handle_toplevel_create(struct zwlr_foreign_toplevel_handle_v1 *tl_handle)
{
    tasks_.push_back(std::make_unique<Task>(bar_, config_, this, tl_handle, seat_));
}

void Taskbar::handle_finished()
{
    zwlr_foreign_toplevel_manager_v1_destroy(manager_);
    manager_ = nullptr;
}

void Taskbar::add_button(Gtk::Button &bt)
{
    box_.pack_start(bt, false, false);
}

void Taskbar::move_button(Gtk::Button &bt, int pos)
{
    box_.reorder_child(bt, pos);
}

void Taskbar::remove_button(Gtk::Button &bt)
{
    box_.remove(bt);
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

bool Taskbar::show_output(struct wl_output *output) const
{
    return output == gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
}

bool Taskbar::all_outputs() const
{
    return config_["all-outputs"].isBool() && config_["all-outputs"].asBool();
}

std::vector<Glib::RefPtr<Gtk::IconTheme>> Taskbar::icon_themes() const
{
    return icon_themes_;
}

} /* namespace waybar::modules::wlr */
