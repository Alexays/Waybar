#include "modules/river/layout.hpp"

#include <spdlog/spdlog.h>
#include <wayland-client.h>

#include "client.hpp"

namespace waybar::modules::river {

static void listen_focused_tags(void *data, struct zriver_output_status_v1 *zriver_output_status_v1,
                                uint32_t tags) {
  // Intentionally empty
}

static void listen_view_tags(void *data, struct zriver_output_status_v1 *zriver_output_status_v1,
                             struct wl_array *tags) {
  // Intentionally empty
}

static void listen_urgent_tags(void *data, struct zriver_output_status_v1 *zriver_output_status_v1,
                               uint32_t tags) {
  // Intentionally empty
}

static void listen_layout_name(void *data, struct zriver_output_status_v1 *zriver_output_status_v1,
                               const char *layout) {
  static_cast<Layout *>(data)->handle_name(layout);
}

static void listen_layout_name_clear(void *data,
                                     struct zriver_output_status_v1 *zriver_output_status_v1) {
  static_cast<Layout *>(data)->handle_clear();
}

static void listen_focused_output(void *data, struct zriver_seat_status_v1 *zriver_seat_status_v1,
                                  struct wl_output *output) {
  static_cast<Layout *>(data)->handle_focused_output(output);
}

static void listen_unfocused_output(void *data, struct zriver_seat_status_v1 *zriver_seat_status_v1,
                                    struct wl_output *output) {
  static_cast<Layout *>(data)->handle_unfocused_output(output);
}

static void listen_focused_view(void *data, struct zriver_seat_status_v1 *zriver_seat_status_v1,
                                const char *title) {
  // Intentionally empty
}

static void listen_mode(void *data, struct zriver_seat_status_v1 *zriver_seat_status_v1,
                        const char *mode) {
  // Intentionally empty
}

static const zriver_output_status_v1_listener output_status_listener_impl{
    .focused_tags = listen_focused_tags,
    .view_tags = listen_view_tags,
    .urgent_tags = listen_urgent_tags,
    .layout_name = listen_layout_name,
    .layout_name_clear = listen_layout_name_clear,
};

static const zriver_seat_status_v1_listener seat_status_listener_impl{
    .focused_output = listen_focused_output,
    .unfocused_output = listen_unfocused_output,
    .focused_view = listen_focused_view,
    .mode = listen_mode,
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
  if (std::strcmp(interface, zriver_status_manager_v1_interface.name) == 0) {
    version = std::min<uint32_t>(version, 4);

    // implies ZRIVER_OUTPUT_STATUS_V1_LAYOUT_NAME_CLEAR_SINCE_VERSION
    if (version < ZRIVER_OUTPUT_STATUS_V1_LAYOUT_NAME_SINCE_VERSION) {
      spdlog::error(
          "river server does not support the \"layout_name\" and \"layout_clear\" events; the "
          "module will be disabled" +
          std::to_string(version));
      return;
    }
    static_cast<Layout *>(data)->status_manager_ = static_cast<struct zriver_status_manager_v1 *>(
        wl_registry_bind(registry, name, &zriver_status_manager_v1_interface, version));
  }

  if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    version = std::min<uint32_t>(version, 1);
    static_cast<Layout *>(data)->seat_ = static_cast<struct wl_seat *>(
        wl_registry_bind(registry, name, &wl_seat_interface, version));
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  // Nobody cares
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

Layout::Layout(const std::string &id, const waybar::Bar &bar, const Json::Value &config)
    : waybar::ALabel(config, "layout", id, "{}"),
      status_manager_{nullptr},
      seat_{nullptr},
      bar_(bar),
      output_status_{nullptr} {
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

  label_.hide();
  ALabel::update();

  seat_status_ = zriver_status_manager_v1_get_river_seat_status(status_manager_, seat_);
  zriver_seat_status_v1_add_listener(seat_status_, &seat_status_listener_impl, this);

  output_status_ = zriver_status_manager_v1_get_river_output_status(status_manager_, output_);
  zriver_output_status_v1_add_listener(output_status_, &output_status_listener_impl, this);

  zriver_status_manager_v1_destroy(status_manager_);
}

Layout::~Layout() {
  if (output_status_) {
    zriver_output_status_v1_destroy(output_status_);
  }
  if (seat_status_) {
    zriver_seat_status_v1_destroy(seat_status_);
  }
}

void Layout::handle_name(const char *name) {
  if (std::strcmp(name, "") == 0 || format_.empty()) {
    label_.hide();  // hide empty labels or labels with empty format
  } else {
    label_.show();
    label_.set_markup(fmt::format(fmt::runtime(format_), Glib::Markup::escape_text(name).raw()));
  }
  ALabel::update();
}

void Layout::handle_clear() {
  label_.hide();
  ALabel::update();
}

void Layout::handle_focused_output(struct wl_output *output) {
  if (output_ == output) {  // if we focused the output this bar belongs to
    label_.get_style_context()->add_class("focused");
    ALabel::update();
  }
  focused_output_ = output;
}

void Layout::handle_unfocused_output(struct wl_output *output) {
  if (output_ == output) {  // if we unfocused the output this bar belongs to
    label_.get_style_context()->remove_class("focused");
    ALabel::update();
  }
}

} /* namespace waybar::modules::river */
