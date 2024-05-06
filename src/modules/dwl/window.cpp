#include "modules/dwl/window.hpp"

#include <gdkmm/pixbuf.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <gtkmm/enums.h>
#include <spdlog/spdlog.h>

#include "client.hpp"
#include "dwl-ipc-unstable-v2-client-protocol.h"
#include "util/rewrite_string.hpp"

namespace waybar::modules::dwl {

static void toggle_visibility(void *data, zdwl_ipc_output_v2 *zdwl_output_v2) {
  // Intentionally empty
}

static void active(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, uint32_t active) {
  // Intentionally empty
}

static void set_tag(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, uint32_t tag, uint32_t state,
                    uint32_t clients, uint32_t focused) {
  // Intentionally empty
}

static void set_layout_symbol(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, const char *layout) {
  static_cast<Window *>(data)->handle_layout_symbol(layout);
}

static void title(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, const char *title) {
  static_cast<Window *>(data)->handle_title(title);
}

static void dwl_frame(void *data, zdwl_ipc_output_v2 *zdwl_output_v2) {
  static_cast<Window *>(data)->handle_frame();
}

static void set_layout(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, uint32_t layout) {
  static_cast<Window *>(data)->handle_layout(layout);
}

static void appid(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, const char *appid) {
  static_cast<Window *>(data)->handle_appid(appid);
};

static const zdwl_ipc_output_v2_listener output_status_listener_impl{
    .toggle_visibility = toggle_visibility,
    .active = active,
    .tag = set_tag,
    .layout = set_layout,
    .title = title,
    .appid = appid,
    .layout_symbol = set_layout_symbol,
    .frame = dwl_frame,
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
  if (std::strcmp(interface, zdwl_ipc_manager_v2_interface.name) == 0) {
    static_cast<Window *>(data)->status_manager_ = static_cast<struct zdwl_ipc_manager_v2 *>(
        (zdwl_ipc_manager_v2 *)wl_registry_bind(registry, name, &zdwl_ipc_manager_v2_interface, 1));
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  /* Ignore event */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

Window::Window(const std::string &id, const Bar &bar, const Json::Value &config)
    : AAppIconLabel(config, "window", id, "{}", 0, true), bar_(bar) {
  struct wl_display *display = Client::inst()->wl_display;
  struct wl_registry *registry = wl_display_get_registry(display);

  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

  if (status_manager_ == nullptr) {
    spdlog::error("dwl_status_manager_v2 not advertised");
    return;
  }

  struct wl_output *output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
  output_status_ = zdwl_ipc_manager_v2_get_output(status_manager_, output);
  zdwl_ipc_output_v2_add_listener(output_status_, &output_status_listener_impl, this);
  zdwl_ipc_manager_v2_destroy(status_manager_);
}

Window::~Window() {
  if (output_status_ != nullptr) {
    zdwl_ipc_output_v2_destroy(output_status_);
  }
}

void Window::handle_title(const char *title) { title_ = title; }

void Window::handle_appid(const char *appid) { appid_ = appid; }

void Window::handle_layout_symbol(const char *layout_symbol) { layout_symbol_ = layout_symbol; }

void Window::handle_layout(const uint32_t layout) { layout_ = layout; }

void Window::handle_frame() {
  label_.set_markup(waybar::util::rewriteString(
      fmt::format(fmt::runtime(format_), fmt::arg("title", title_),
                  fmt::arg("layout", layout_symbol_), fmt::arg("app_id", appid_)),
      config_["rewrite"]));
  updateAppIconName(appid_, "");
  updateAppIcon();
  if (tooltipEnabled()) {
    label_.set_tooltip_text(title_);
  }
}

}  // namespace waybar::modules::dwl
