#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>
#include <wayland-client.h>

#include "client.hpp"
#include "modules/river/tags.hpp"
#include "river-status-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace waybar::modules::river {

static void listen_focused_tags(void *data, struct zriver_output_status_v1 *zriver_output_status_v1,
                                uint32_t tags) {
  static_cast<Tags *>(data)->handle_focused_tags(tags);
}

static void listen_view_tags(void *data, struct zriver_output_status_v1 *zriver_output_status_v1,
                             struct wl_array *tags) {
  static_cast<Tags *>(data)->handle_view_tags(tags);
}

static const zriver_output_status_v1_listener output_status_listener_impl{
    .focused_tags = listen_focused_tags,
    .view_tags = listen_view_tags,
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
  if (std::strcmp(interface, zriver_status_manager_v1_interface.name) == 0) {
    static_cast<Tags *>(data)->status_manager_ = static_cast<struct zriver_status_manager_v1 *>(
        wl_registry_bind(registry, name, &zriver_status_manager_v1_interface, version));
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  /* Ignore event */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

Tags::Tags(const std::string &id, const waybar::Bar &bar, const Json::Value &config)
    : waybar::AModule(config, "tags", id, false, false),
      status_manager_{nullptr},
      bar_(bar),
      box_{bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0},
      output_status_{nullptr} {
  struct wl_display * display = Client::inst()->wl_display;
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

  if (!status_manager_) {
    spdlog::error("river_status_manager_v1 not advertised");
    return;
  }

  box_.set_name("tags");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);

  // Default to 9 tags
  const uint32_t num_tags = config["num-tags"].isUInt() ? config_["num-tags"].asUInt() : 9;
  for (uint32_t tag = 1; tag <= num_tags; ++tag) {
    Gtk::Button &button = buttons_.emplace_back(std::to_string(tag));
    button.set_relief(Gtk::RELIEF_NONE);
    box_.pack_start(button, false, false, 0);
    button.show();
  }

  struct wl_output *output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
  output_status_ = zriver_status_manager_v1_get_river_output_status(status_manager_, output);
  zriver_output_status_v1_add_listener(output_status_, &output_status_listener_impl, this);

  zriver_status_manager_v1_destroy(status_manager_);
}

Tags::~Tags() {
  if (output_status_) {
    zriver_output_status_v1_destroy(output_status_);
  }
}

void Tags::handle_focused_tags(uint32_t tags) {
  uint32_t i = 0;
  for (auto &button : buttons_) {
    if ((1 << i) & tags) {
      button.get_style_context()->add_class("focused");
    } else {
      button.get_style_context()->remove_class("focused");
    }
    ++i;
  }
}

void Tags::handle_view_tags(struct wl_array *view_tags) {
  // First clear all occupied state
  for (auto &button : buttons_) {
    button.get_style_context()->remove_class("occupied");
  }

  // Set tags with a view to occupied
  uint32_t *start = static_cast<uint32_t *>(view_tags->data);
  for (uint32_t *tags = start; tags < start + view_tags->size / sizeof(uint32_t); ++tags) {
    uint32_t i = 0;
    for (auto &button : buttons_) {
      if (*tags & (1 << i)) {
        button.get_style_context()->add_class("occupied");
      }
      ++i;
    }
  }
}

} /* namespace waybar::modules::river */
