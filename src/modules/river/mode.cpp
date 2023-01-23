#include "modules/river/mode.hpp"

#include <spdlog/spdlog.h>
#include <wayland-client.h>

#include "client.hpp"

namespace waybar::modules::river {

static void listen_focused_output(void *data, struct zriver_seat_status_v1 *seat_status,
                                  struct wl_output *output) {
  // Intentionally empty
}

static void listen_unfocused_output(void *data, struct zriver_seat_status_v1 *seat_status,
                                    struct wl_output *output) {
  // Intentionally empty
}

static void listen_focused_view(void *data, struct zriver_seat_status_v1 *seat_status,
                                const char *title) {
  // Intentionally empty
}

static void listen_mode(void *data, struct zriver_seat_status_v1 *seat_status, const char *mode) {
  static_cast<Mode *>(data)->handle_mode(mode);
}

static const zriver_seat_status_v1_listener seat_status_listener_impl = {
    .focused_output = listen_focused_output,
    .unfocused_output = listen_unfocused_output,
    .focused_view = listen_focused_view,
    .mode = listen_mode,
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
  if (std::strcmp(interface, zriver_status_manager_v1_interface.name) == 0) {
    version = std::min<uint32_t>(version, 3);
    if (version < ZRIVER_SEAT_STATUS_V1_MODE_SINCE_VERSION) {
      spdlog::error(
          "river server does not support the \"mode\" event; the module will be disabled");
      return;
    }
    static_cast<Mode *>(data)->status_manager_ = static_cast<struct zriver_status_manager_v1 *>(
        wl_registry_bind(registry, name, &zriver_status_manager_v1_interface, version));
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    version = std::min<uint32_t>(version, 1);
    static_cast<Mode *>(data)->seat_ = static_cast<struct wl_seat *>(
        wl_registry_bind(registry, name, &wl_seat_interface, version));
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  // Nobody cares
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

Mode::Mode(const std::string &id, const waybar::Bar &bar, const Json::Value &config)
    : waybar::ALabel(config, "mode", id, "{}"),
      status_manager_{nullptr},
      seat_{nullptr},
      bar_(bar),
      mode_{""},
      seat_status_{nullptr} {
  struct wl_display *display = Client::inst()->wl_display;
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

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

  zriver_status_manager_v1_destroy(status_manager_);
}

Mode::~Mode() {
  if (seat_status_) {
    zriver_seat_status_v1_destroy(seat_status_);
  }
}

void Mode::handle_mode(const char *mode) {
  if (format_.empty()) {
    label_.hide();
  } else {
    if (!mode_.empty()) {
      label_.get_style_context()->remove_class(mode_);
    }

    label_.get_style_context()->add_class(mode);
    label_.set_markup(fmt::format(fmt::runtime(format_), Glib::Markup::escape_text(mode).raw()));
    label_.show();
  }

  mode_ = mode;
  ALabel::update();
}

} /* namespace waybar::modules::river */
