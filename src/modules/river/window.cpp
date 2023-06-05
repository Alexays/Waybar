#include "modules/river/window.hpp"

#include <spdlog/spdlog.h>
#include <wayland-client.h>

#include <algorithm>

#include "client.hpp"

namespace waybar::modules::river {

static void listen_focused_view(void *data, struct zriver_seat_status_v1 *zriver_seat_status_v1,
                                const char *title) {
  static_cast<Window *>(data)->handle_focused_view(title);
}

static void listen_focused_output(void *data, struct zriver_seat_status_v1 *zriver_seat_status_v1,
                                  struct wl_output *output) {
  static_cast<Window *>(data)->handle_focused_output(output);
}

static void listen_unfocused_output(void *data, struct zriver_seat_status_v1 *zriver_seat_status_v1,
                                    struct wl_output *output) {
  static_cast<Window *>(data)->handle_unfocused_output(output);
}

static void listen_mode(void *data, struct zriver_seat_status_v1 *zriver_seat_status_v1,
                        const char *mode) {
  // This module doesn't care
}

static const zriver_seat_status_v1_listener seat_status_listener_impl{
    .focused_output = listen_focused_output,
    .unfocused_output = listen_unfocused_output,
    .focused_view = listen_focused_view,
    .mode = listen_mode,
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
  if (std::strcmp(interface, zriver_status_manager_v1_interface.name) == 0) {
    version = std::min<uint32_t>(version, 2);
    static_cast<Window *>(data)->status_manager_ = static_cast<struct zriver_status_manager_v1 *>(
        wl_registry_bind(registry, name, &zriver_status_manager_v1_interface, version));
  }

  if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    version = std::min<uint32_t>(version, 1);
    static_cast<Window *>(data)->seat_ = static_cast<struct wl_seat *>(
        wl_registry_bind(registry, name, &wl_seat_interface, version));
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  /* Ignore event */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

Window::Window(const std::string &id, const waybar::Bar &bar, const Json::Value &config)
    : waybar::ALabel(config, "window", id, "{}", 30),
      status_manager_{nullptr},
      seat_{nullptr},
      bar_(bar),
      seat_status_{nullptr} {
  struct wl_display *display = Client::inst()->wl_display;
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

  output_ = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());

  if (!status_manager_) {
    spdlog::error("river_status_manager_v1 not advertised");
    return;
  }

  if (!seat_) {
    spdlog::error("wl_seat not advertised");
  }

  label_.hide();  // hide the label until populated
  ALabel::update();

  seat_status_ = zriver_status_manager_v1_get_river_seat_status(status_manager_, seat_);
  zriver_seat_status_v1_add_listener(seat_status_, &seat_status_listener_impl, this);

  zriver_status_manager_v1_destroy(status_manager_);
}

Window::~Window() {
  if (seat_status_) {
    zriver_seat_status_v1_destroy(seat_status_);
  }
}

void Window::handle_focused_view(const char *title) {
  // don't change the label on unfocused outputs.
  // this makes the current output report its currently focused view, and unfocused outputs will
  // report their last focused views. when freshly starting the bar, unfocused outputs don't have a
  // last focused view, and will get blank labels until they are brought into focus at least once.
  if (focused_output_ != output_) return;

  if (std::strcmp(title, "") == 0 || format_.empty()) {
    label_.hide();  // hide empty labels or labels with empty format
  } else {
    label_.show();
    auto text = fmt::format(fmt::runtime(format_), Glib::Markup::escape_text(title).raw());
    label_.set_markup(text);
    if (tooltipEnabled()) {
      label_.set_tooltip_markup(text);
    }
  }

  ALabel::update();
}

void Window::handle_focused_output(struct wl_output *output) {
  if (output_ == output) {  // if we focused the output this bar belongs to
    label_.get_style_context()->add_class("focused");
    ALabel::update();
  }
  focused_output_ = output;
}

void Window::handle_unfocused_output(struct wl_output *output) {
  if (output_ == output) {  // if we unfocused the output this bar belongs to
    label_.get_style_context()->remove_class("focused");
    ALabel::update();
  }
}

} /* namespace waybar::modules::river */
