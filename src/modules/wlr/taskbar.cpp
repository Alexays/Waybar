#include "modules/wlr/taskbar.hpp"

#include <gio/gdesktopappinfo.h>
#include <glibmm/fileutils.h>
#include <glibmm/markup.h>
#include <gtkmm/dragsource.h>
#include <gtkmm/droptarget.h>
#include <spdlog/spdlog.h>

#include "client.hpp"
#include "util/rewrite_string.hpp"
#include "util/string.hpp"

namespace waybar::modules::wlr {

/* Icon loading functions */
static std::vector<std::string> search_prefix() {
  std::vector<std::string> prefixes = {""};

  std::string home_dir = std::getenv("HOME");
  prefixes.push_back(home_dir + "/.local/share/");

  auto xdg_data_dirs = std::getenv("XDG_DATA_DIRS");
  if (!xdg_data_dirs) {
    prefixes.emplace_back("/usr/share/");
    prefixes.emplace_back("/usr/local/share/");
  } else {
    std::string xdg_data_dirs_str(xdg_data_dirs);
    size_t start = 0, end = 0;

    do {
      end = xdg_data_dirs_str.find(':', start);
      auto p = xdg_data_dirs_str.substr(start, end - start);
      prefixes.push_back(trim(p) + "/");

      start = end == std::string::npos ? end : end + 1;
    } while (end != std::string::npos);
  }

  for (auto &p : prefixes) spdlog::debug("Using 'desktop' search path prefix: {}", p);

  return prefixes;
}

static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_by_name(const std::string &app_id) {
  static std::vector<std::string> prefixes = search_prefix();

  std::vector<std::string> app_folders = {"", "applications/", "applications/kde/",
                                          "applications/org.kde."};

  std::vector<std::string> suffixes = {"", ".desktop"};

  for (auto &prefix : prefixes) {
    for (auto &folder : app_folders) {
      for (auto &suffix : suffixes) {
        auto app_info_ =
            Gio::DesktopAppInfo::create_from_filename(prefix + folder + app_id + suffix);
        if (!app_info_) {
          continue;
        }

        return app_info_;
      }
    }
  }

  return {};
}

Glib::RefPtr<Gio::DesktopAppInfo> get_desktop_app_info(const std::string &app_id) {
  auto app_info = get_app_info_by_name(app_id);
  if (app_info) {
    return app_info;
  }

  std::string desktop_file = "";

  gchar ***desktop_list = g_desktop_app_info_search(app_id.c_str());
  if (desktop_list != nullptr && desktop_list[0] != nullptr) {
    for (size_t i = 0; desktop_list[0][i]; i++) {
      if (desktop_file == "") {
        desktop_file = desktop_list[0][i];
      } else {
        auto tmp_info = Gio::DesktopAppInfo::create(desktop_list[0][i]);
        if (!tmp_info)
          // see https://github.com/Alexays/Waybar/issues/1446
          continue;

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

  return get_app_info_by_name(desktop_file);
}

void Task::set_app_info_from_app_id_list(const std::string &app_id_list) {
  std::string app_id;
  std::istringstream stream(app_id_list);

  /* Wayfire sends a list of app-id's in space separated format, other compositors
   * send a single app-id, but in any case this works fine */
  while (stream >> app_id) {
    app_info_ = get_desktop_app_info(app_id);
    if (app_info_) {
      return;
    }

    auto lower_app_id = app_id;
    std::transform(lower_app_id.begin(), lower_app_id.end(), lower_app_id.begin(),
                   [](char c) { return std::tolower(c); });
    app_info_ = get_desktop_app_info(lower_app_id);
    if (app_info_) {
      return;
    }

    size_t start = 0, end = app_id.size();
    start = app_id.rfind(".", end);
    std::string app_name = app_id.substr(start + 1, app_id.size());
    app_info_ = get_desktop_app_info(app_name);
    if (app_info_) {
      return;
    }

    start = app_id.find("-");
    app_name = app_id.substr(0, start);
    app_info_ = get_desktop_app_info(app_name);
  }
}

static std::string get_icon_name_from_icon_theme(const Glib::RefPtr<Gtk::IconTheme> &icon_theme,
                                                 const std::string &app_id) {
  if (icon_theme->lookup_icon(app_id, 24)) return app_id;

  return "";
}

bool Task::image_load_icon(Gtk::Image &image, const Glib::RefPtr<Gtk::IconTheme> &icon_theme,
                           Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size) {
  Glib::ustring iconName{(app_info) ? app_info->get_icon()->to_string() : "unknown"};
  if (app_info && iconName.empty())
    iconName = get_icon_name_from_icon_theme(icon_theme, app_info->get_startup_wm_class());
  if (iconName.empty()) iconName = "unknown";

  auto themeIcon{
      icon_theme->lookup_icon((icon_theme->has_icon(iconName)) ? iconName : "image-missing", size,
                              (int)image.get_scale_factor(), Gtk::TextDirection::NONE,
                              Gtk::IconLookupFlags::FORCE_REGULAR)};
  if (!(icon_theme->has_icon(iconName))) {
    if (Glib::file_test(iconName, Glib::FileTest::EXISTS)) {
      themeIcon = Gtk::IconPaintable::create(Gio::File::create_for_path(iconName), size,
                                             image.get_scale_factor());
    }
  }

  if (themeIcon) {
    image.set(themeIcon);

    return true;
  }

  return false;
}

/* Task class implementation */
uint32_t Task::global_id = 0;

static void tl_handle_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                            const char *title) {
  return static_cast<Task *>(data)->handle_title(title);
}

static void tl_handle_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                             const char *app_id) {
  return static_cast<Task *>(data)->handle_app_id(app_id);
}

static void tl_handle_output_enter(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                                   struct wl_output *output) {
  return static_cast<Task *>(data)->handle_output_enter(output);
}

static void tl_handle_output_leave(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                                   struct wl_output *output) {
  return static_cast<Task *>(data)->handle_output_leave(output);
}

static void tl_handle_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                            struct wl_array *state) {
  return static_cast<Task *>(data)->handle_state(state);
}

static void tl_handle_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle) {
  return static_cast<Task *>(data)->handle_done();
}

static void tl_handle_parent(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                             struct zwlr_foreign_toplevel_handle_v1 *parent) {
  /* This is explicitly left blank */
}

static void tl_handle_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle) {
  return static_cast<Task *>(data)->handle_closed();
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
           struct zwlr_foreign_toplevel_handle_v1 *tl_handle, struct wl_seat *seat)
    : button{new Gtk::Button()},
      controllClick_{Gtk::GestureClick::create()},
      bar_{bar},
      config_{config},
      tbar_{tbar},
      handle_{tl_handle},
      seat_{seat},
      id_{global_id++},
      content_{bar.orientation, 0} {
  zwlr_foreign_toplevel_handle_v1_add_listener(handle_, &toplevel_handle_impl, this);

  button->set_has_frame(false);

  content_.append(text_before_);
  content_.append(icon_);
  content_.append(text_after_);

  content_.show();
  button->set_child(content_);

  format_before_.clear();
  format_after_.clear();

  if (config_["format"].isString()) {
    /* The user defined a format string, use it */
    auto format = config_["format"].asString();
    if (format.find("{name}") != std::string::npos) {
      with_name_ = true;
    }

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

  if (app_id_.empty()) {
    handle_app_id("unknown");
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
  if (config_["on-click"].isString() || config_["on-click-middle"].isString() ||
      config_["on-click-right"].isString()) {
    controllClick_->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    controllClick_->signal_released().connect(sigc::mem_fun(*this, &Task::handleClick), false);
    button->add_controller(controllClick_);
  }

  // Setup DND source
  Glib::Value<Glib::RefPtr<Gtk::Button>> buttonValue;
  auto source = Gtk::DragSource::create();
  source->set_actions(Gdk::DragAction::COPY);
  buttonValue.init(buttonValue.value_type());
  buttonValue.set(button);
  source->set_content(Gdk::ContentProvider::create(buttonValue));
  button->add_controller(source);
  // Setup DND target
  const GType target_type = Glib::Value<Glib::RefPtr<Gtk::Button>>::value_type();
  auto target = Gtk::DropTarget::create(target_type, Gdk::DragAction::COPY);
  target->signal_drop().connect(sigc::mem_fun(*this, &Task::handleDropData), false);
  button->add_controller(target);
}

Task::~Task() {
  if (handle_) {
    zwlr_foreign_toplevel_handle_v1_destroy(handle_);
    handle_ = nullptr;
  }
  if (button_visible_) {
    tbar_->remove_button(button);
    button_visible_ = false;
  }
}

std::string Task::repr() const {
  std::stringstream ss;
  ss << "Task (" << id_ << ") " << title_ << " [" << app_id_ << "] <" << (active() ? "A" : "a")
     << (maximized() ? "M" : "m") << (minimized() ? "I" : "i") << (fullscreen() ? "F" : "f") << ">";

  return ss.str();
}

std::string Task::state_string(bool shortened) const {
  std::stringstream ss;
  if (shortened)
    ss << (minimized() ? "m" : "") << (maximized() ? "M" : "") << (active() ? "A" : "")
       << (fullscreen() ? "F" : "");
  else
    ss << (minimized() ? "minimized " : "") << (maximized() ? "maximized " : "")
       << (active() ? "active " : "") << (fullscreen() ? "fullscreen " : "");

  std::string res = ss.str();
  if (shortened || res.empty())
    return res;
  else
    return res.substr(0, res.size() - 1);
}

void Task::handle_title(const char *title) {
  title_ = title;
  hide_if_ignored();
}

void Task::set_minimize_hint() {
  zwlr_foreign_toplevel_handle_v1_set_rectangle(handle_, bar_.surface, minimize_hint.x,
                                                minimize_hint.y, minimize_hint.w, minimize_hint.h);
}

void Task::hide_if_ignored() {
  if (tbar_->ignore_list().count(app_id_) || tbar_->ignore_list().count(title_)) {
    ignored_ = true;
    if (button_visible_) {
      auto output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
      handle_output_leave(output);
    }
  } else {
    bool is_was_ignored = ignored_;
    ignored_ = false;
    if (is_was_ignored) {
      auto output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
      handle_output_enter(output);
    }
  }
}

void Task::handle_app_id(const char *app_id) {
  if (app_id_.empty()) {
    spdlog::debug(fmt::format("Task ({}) setting app_id to {}", id_, app_id));
  } else {
    spdlog::debug(fmt::format("Task ({}) overwriting app_id '{}' with '{}'", id_, app_id_, app_id));
  }
  app_id_ = app_id;
  hide_if_ignored();

  auto ids_replace_map = tbar_->app_ids_replace_map();
  if (ids_replace_map.count(app_id_)) {
    auto replaced_id = ids_replace_map[app_id_];
    spdlog::debug(
        fmt::format("Task ({}) [{}] app_id was replaced with {}", id_, app_id_, replaced_id));
    app_id_ = replaced_id;
  }

  if (!with_icon_ && !with_name_) {
    return;
  }

  set_app_info_from_app_id_list(app_id_);
  name_ = app_info_ ? app_info_->get_display_name() : app_id;

  if (!with_icon_) {
    return;
  }

  int icon_size = config_["icon-size"].isInt() ? config_["icon-size"].asInt() : 16;
  bool found = false;
  for (auto &icon_theme : tbar_->icon_themes()) {
    if (image_load_icon(icon_, icon_theme, app_info_, icon_size)) {
      found = true;
      break;
    }
  }

  if (found)
    icon_.show();
  else
    spdlog::debug("Couldn't find icon for {}", app_id_);
}

void Task::handle_output_enter(struct wl_output *output) {
  if (ignored_) {
    spdlog::debug("{} is ignored", repr());
    return;
  }

  spdlog::debug("{} entered output {}", repr(), (void *)output);

  if (!button_visible_ && (tbar_->all_outputs() || tbar_->show_output(output))) {
    /* The task entered the output of the current bar make the button visible */
    button->property_width_request().signal_changed().connect(
        [&]() { minimize_hint.w = button->get_width(); });
    button->property_height_request().signal_changed().connect(
        [&]() { minimize_hint.h = button->get_height(); });

    tbar_->add_button(button);
    button->show();
    button_visible_ = true;
    spdlog::debug("{} now visible on {}", repr(), bar_.output->name);
  }
}

void Task::handle_output_leave(struct wl_output *output) {
  spdlog::debug("{} left output {}", repr(), (void *)output);

  if (button_visible_ && !tbar_->all_outputs() && tbar_->show_output(output)) {
    /* The task left the output of the current bar, make the button invisible */
    tbar_->remove_button(button);
    button->hide();
    button_visible_ = false;
    spdlog::debug("{} now invisible on {}", repr(), bar_.output->name);
  }
}

void Task::handle_state(struct wl_array *state) {
  state_ = 0;
  size_t size = state->size / sizeof(uint32_t);
  for (size_t i = 0; i < size; ++i) {
    auto entry = static_cast<uint32_t *>(state->data)[i];
    if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED) state_ |= MAXIMIZED;
    if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED) state_ |= MINIMIZED;
    if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) state_ |= ACTIVE;
    if (entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN) state_ |= FULLSCREEN;
  }
}

void Task::handle_done() {
  spdlog::debug("{} changed", repr());

  if (state_ & MAXIMIZED) {
    button->add_css_class("maximized");
  } else if (!(state_ & MAXIMIZED)) {
    button->remove_css_class("maximized");
  }

  if (state_ & MINIMIZED) {
    button->add_css_class("minimized");
  } else if (!(state_ & MINIMIZED)) {
    button->remove_css_class("minimized");
  }

  if (state_ & ACTIVE) {
    button->add_css_class("active");
  } else if (!(state_ & ACTIVE)) {
    button->remove_css_class("active");
  }

  if (state_ & FULLSCREEN) {
    button->add_css_class("fullscreen");
  } else if (!(state_ & FULLSCREEN)) {
    button->remove_css_class("fullscreen");
  }

  if (config_["active-first"].isBool() && config_["active-first"].asBool() && active())
    tbar_->move_button(button, 0);

  tbar_->dp.emit();
}

void Task::handle_closed() {
  spdlog::debug("{} closed", repr());
  zwlr_foreign_toplevel_handle_v1_destroy(handle_);
  handle_ = nullptr;
  if (button_visible_) {
    tbar_->remove_button(button);
    button_visible_ = false;
  }
  tbar_->remove_task(id_);
}

void Task::handleClick(int n_press, double dx, double dy) {
  auto currButton{controllClick_->get_current_button()};

  std::string action;
  if (config_["on-click"].isString() && currButton == 1)
    action = config_["on-click"].asString();
  else if (config_["on-click-middle"].isString() && currButton == 2)
    action = config_["on-click-middle"].asString();
  else if (config_["on-click-right"].isString() && currButton == 3)
    action = config_["on-click-right"].asString();

  if (action.empty())
    return;
  else if (action == "activate")
    activate();
  else if (action == "minimize") {
    set_minimize_hint();
    minimize(!minimized());
  } else if (action == "minimize-raise") {
    set_minimize_hint();
    if (minimized())
      minimize(false);
    else if (active())
      minimize(true);
    else
      activate();
  } else if (action == "maximize")
    maximize(!maximized());
  else if (action == "fullscreen")
    fullscreen(!fullscreen());
  else if (action == "close")
    close();
  else
    spdlog::warn("Unknown action {}", action);
}

bool Task::handleDropData(const Glib::ValueBase &value, double, double) {
  if (G_VALUE_HOLDS(value.gobj(), Glib::Value<Glib::RefPtr<Gtk::Button>>::value_type())) {
    // Expand incoming value
    Glib::Value<Glib::RefPtr<Gtk::Button>> dropValue;
    dropValue.init(value.gobj());
    const Glib::RefPtr<Gtk::Button> draggedButton = dropValue.get();

    if (draggedButton) {
      if (draggedButton != this->button) {
        auto draggedParent{draggedButton->get_parent()};
        auto droppedParent{this->button->get_parent()};
        if (draggedParent == droppedParent) {
          auto draggedBox{(Gtk::Box *)draggedParent};
          // Need to move dragged button before the current
          draggedBox->reorder_child_after(*draggedButton, *this->button);
          draggedBox->reorder_child_after(*this->button, *draggedButton);
        }
      }
    }

    return true;
  } else {
    spdlog::warn("WLR taskbar. Received unexpected data type {} in button",
                 G_VALUE_TYPE_NAME(value.gobj()));
    return false;
  }
}

bool Task::operator==(const Task &o) const { return o.id_ == id_; }

bool Task::operator!=(const Task &o) const { return o.id_ != id_; }

void Task::update() {
  bool markup = config_["markup"].isBool() ? config_["markup"].asBool() : false;
  std::string title = title_;
  std::string name = name_;
  std::string app_id = app_id_;
  if (markup) {
    title = Glib::Markup::escape_text(title);
    name = Glib::Markup::escape_text(name);
    app_id = Glib::Markup::escape_text(app_id);
  }
  if (!format_before_.empty()) {
    auto txt =
        fmt::format(fmt::runtime(format_before_), fmt::arg("title", title), fmt::arg("name", name),
                    fmt::arg("app_id", app_id), fmt::arg("state", state_string()),
                    fmt::arg("short_state", state_string(true)));

    txt = waybar::util::rewriteString(txt, config_["rewrite"]);

    if (markup)
      text_before_.set_markup(txt);
    else
      text_before_.set_label(txt);
    text_before_.show();
  }
  if (!format_after_.empty()) {
    auto txt =
        fmt::format(fmt::runtime(format_after_), fmt::arg("title", title), fmt::arg("name", name),
                    fmt::arg("app_id", app_id), fmt::arg("state", state_string()),
                    fmt::arg("short_state", state_string(true)));

    txt = waybar::util::rewriteString(txt, config_["rewrite"]);

    if (markup)
      text_after_.set_markup(txt);
    else
      text_after_.set_label(txt);
    text_after_.show();
  }

  if (!format_tooltip_.empty()) {
    auto txt =
        fmt::format(fmt::runtime(format_tooltip_), fmt::arg("title", title), fmt::arg("name", name),
                    fmt::arg("app_id", app_id), fmt::arg("state", state_string()),
                    fmt::arg("short_state", state_string(true)));
    if (markup)
      button->set_tooltip_markup(txt);
    else
      button->set_tooltip_text(txt);
  }
}

void Task::maximize(bool set) {
  if (set)
    zwlr_foreign_toplevel_handle_v1_set_maximized(handle_);
  else
    zwlr_foreign_toplevel_handle_v1_unset_maximized(handle_);
}

void Task::minimize(bool set) {
  if (set)
    zwlr_foreign_toplevel_handle_v1_set_minimized(handle_);
  else
    zwlr_foreign_toplevel_handle_v1_unset_minimized(handle_);
}

void Task::activate() { zwlr_foreign_toplevel_handle_v1_activate(handle_, seat_); }

void Task::fullscreen(bool set) {
  if (zwlr_foreign_toplevel_handle_v1_get_version(handle_) <
      ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_SET_FULLSCREEN_SINCE_VERSION) {
    spdlog::warn("Foreign toplevel manager server does not support for set/unset fullscreen.");
    return;
  }

  if (set)
    zwlr_foreign_toplevel_handle_v1_set_fullscreen(handle_, nullptr);
  else
    zwlr_foreign_toplevel_handle_v1_unset_fullscreen(handle_);
}

void Task::close() { zwlr_foreign_toplevel_handle_v1_close(handle_); }

/* Taskbar class implementation */
static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
  if (std::strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
    static_cast<Taskbar *>(data)->register_manager(registry, name, version);
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    static_cast<Taskbar *>(data)->register_seat(registry, name, version);
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  /* Nothing to do here */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

Taskbar::Taskbar(const std::string &id, const waybar::Bar &bar, const Json::Value &config)
    : waybar::AModule(config, "taskbar", id, false, false),
      bar_(bar),
      box_{bar.orientation, 0},
      manager_{nullptr},
      seat_{nullptr} {
  box_.set_name("taskbar");
  if (!id.empty()) {
    box_.add_css_class(id);
  }
  box_.add_css_class(MODULE_CLASS);
  box_.add_css_class("empty");

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
    for (auto &c : config_["icon-theme"]) {
      auto it_name = c.asString();

      auto it = Gtk::IconTheme::create();
      it->set_theme_name(it_name);
      spdlog::debug("Use custom icon theme: {}", it_name);

      icon_themes_.push_back(it);
    }
  } else if (config_["icon-theme"].isString()) {
    auto it_name = config_["icon-theme"].asString();

    auto it = Gtk::IconTheme::create();
    it->set_theme_name(it_name);
    spdlog::debug("Use custom icon theme: {}", it_name);

    icon_themes_.push_back(it);
  }

  // Load ignore-list
  if (config_["ignore-list"].isArray()) {
    for (auto &app_name : config_["ignore-list"]) {
      ignore_list_.emplace(app_name.asString());
    }
  }

  // Load app_id remappings
  if (config_["app_ids-mapping"].isObject()) {
    const Json::Value &mapping = config_["app_ids-mapping"];
    const std::vector<std::string> app_ids = config_["app_ids-mapping"].getMemberNames();
    for (auto &app_id : app_ids) {
      app_ids_replace_map_.emplace(app_id, mapping[app_id].asString());
    }
  }

  icon_themes_.push_back(Gtk::IconTheme::get_for_display(box_.get_display()));

  for (auto &t : tasks_) {
    t->handle_app_id(t->app_id().c_str());
  }
}

Taskbar::~Taskbar() {
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

void Taskbar::update() {
  for (auto &t : tasks_) {
    t->update();
  }

  if (config_["sort-by-app-id"].asBool()) {
    std::stable_sort(tasks_.begin(), tasks_.end(),
                     [](const std::unique_ptr<Task> &a, const std::unique_ptr<Task> &b) {
                       return a->app_id() < b->app_id();
                     });

    for (unsigned long i = 0; i < tasks_.size(); i++) {
      move_button(tasks_[i]->button, i);
    }
  }

  AModule::update();
}

Gtk::Widget &Taskbar::root() { return box_; }

static void tm_handle_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager,
                               struct zwlr_foreign_toplevel_handle_v1 *tl_handle) {
  return static_cast<Taskbar *>(data)->handle_toplevel_create(tl_handle);
}

static void tm_handle_finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager) {
  return static_cast<Taskbar *>(data)->handle_finished();
}

static const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_impl = {
    .toplevel = tm_handle_toplevel,
    .finished = tm_handle_finished,
};

void Taskbar::register_manager(struct wl_registry *registry, uint32_t name, uint32_t version) {
  if (manager_) {
    spdlog::warn("Register foreign toplevel manager again although already existing!");
    return;
  }
  if (version < ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_SET_FULLSCREEN_SINCE_VERSION) {
    spdlog::warn(
        "Foreign toplevel manager server does not have the appropriate version."
        " To be able to use all features, you need at least version 2, but server is version {}",
        version);
  }

  // limit version to a highest supported by the client protocol file
  version = std::min<uint32_t>(version, zwlr_foreign_toplevel_manager_v1_interface.version);

  manager_ = static_cast<struct zwlr_foreign_toplevel_manager_v1 *>(
      wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, version));

  if (manager_)
    zwlr_foreign_toplevel_manager_v1_add_listener(manager_, &toplevel_manager_impl, this);
  else
    spdlog::debug("Failed to register manager");
}

void Taskbar::register_seat(struct wl_registry *registry, uint32_t name, uint32_t version) {
  if (seat_) {
    spdlog::warn("Register seat again although already existing!");
    return;
  }
  version = std::min<uint32_t>(version, wl_seat_interface.version);

  seat_ = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, version));
}

void Taskbar::handle_toplevel_create(struct zwlr_foreign_toplevel_handle_v1 *tl_handle) {
  tasks_.push_back(std::make_unique<Task>(bar_, config_, this, tl_handle, seat_));
}

void Taskbar::handle_finished() {
  zwlr_foreign_toplevel_manager_v1_destroy(manager_);
  manager_ = nullptr;
}

void Taskbar::add_button(Glib::RefPtr<Gtk::Button> bt) {
  box_.append(*bt);
  box_.remove_css_class("empty");
}

void Taskbar::move_button(Glib::RefPtr<Gtk::Button> bt, int pos) {
  try {
    const auto sibling{(box_.get_children()).at(pos - 1)};

    if (sibling) box_.reorder_child_after(*bt, *sibling);
  } catch (const std::out_of_range &ex) {
    spdlog::error("WLR taskbar. {}", ex.what());
  }
}

void Taskbar::remove_button(Glib::RefPtr<Gtk::Button> bt) {
  box_.remove(*bt);
  if (box_.get_children().empty()) {
    box_.add_css_class("empty");
  }
}

void Taskbar::remove_task(uint32_t id) {
  auto it = std::find_if(std::begin(tasks_), std::end(tasks_),
                         [id](const TaskPtr &p) { return p->id() == id; });

  if (it == std::end(tasks_)) {
    spdlog::warn("Can't find task with id {}", id);
    return;
  }

  tasks_.erase(it);
}

bool Taskbar::show_output(struct wl_output *output) const {
  return output == gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
}

bool Taskbar::all_outputs() const {
  return config_["all-outputs"].isBool() && config_["all-outputs"].asBool();
}

const std::vector<Glib::RefPtr<Gtk::IconTheme>> &Taskbar::icon_themes() const {
  return icon_themes_;
}

const std::unordered_set<std::string> &Taskbar::ignore_list() const { return ignore_list_; }

const std::map<std::string, std::string> &Taskbar::app_ids_replace_map() const {
  return app_ids_replace_map_;
}

} /* namespace waybar::modules::wlr */
