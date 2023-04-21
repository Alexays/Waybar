#include "modules/river/tags.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>
#include <wayland-client.h>

#include <algorithm>

#include "client.hpp"
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

static void listen_urgent_tags(void *data, struct zriver_output_status_v1 *zriver_output_status_v1,
                               uint32_t tags) {
  static_cast<Tags *>(data)->handle_urgent_tags(tags);
}

static const zriver_output_status_v1_listener output_status_listener_impl{
    .focused_tags = listen_focused_tags,
    .view_tags = listen_view_tags,
    .urgent_tags = listen_urgent_tags,
};

static void listen_command_success(void *data,
                                   struct zriver_command_callback_v1 *zriver_command_callback_v1,
                                   const char *output) {
  // Do nothing but keep listener to avoid crashing when command was successful
}

static void listen_command_failure(void *data,
                                   struct zriver_command_callback_v1 *zriver_command_callback_v1,
                                   const char *output) {
  spdlog::error("failure when selecting/toggling tags {}", output);
}

static const zriver_command_callback_v1_listener command_callback_listener_impl{
    .success = listen_command_success,
    .failure = listen_command_failure,
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
  if (std::strcmp(interface, zriver_status_manager_v1_interface.name) == 0) {
    version = std::min(version, 2u);
    if (version < ZRIVER_OUTPUT_STATUS_V1_URGENT_TAGS_SINCE_VERSION) {
      spdlog::warn("river server does not support urgent tags");
    }
    static_cast<Tags *>(data)->status_manager_ = static_cast<struct zriver_status_manager_v1 *>(
        wl_registry_bind(registry, name, &zriver_status_manager_v1_interface, version));
  }

  if (std::strcmp(interface, zriver_control_v1_interface.name) == 0) {
    version = std::min(version, 1u);
    static_cast<Tags *>(data)->control_ = static_cast<struct zriver_control_v1 *>(
        wl_registry_bind(registry, name, &zriver_control_v1_interface, version));
  }

  if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    version = std::min(version, 1u);
    static_cast<Tags *>(data)->seat_ = static_cast<struct wl_seat *>(
        wl_registry_bind(registry, name, &wl_seat_interface, version));
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
      control_{nullptr},
      seat_{nullptr},
      bar_(bar),
      box_{bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0},
      output_status_{nullptr} {
  struct wl_display *display = Client::inst()->wl_display;
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

  if (!status_manager_) {
    spdlog::error("river_status_manager_v1 not advertised");
    return;
  }

  if (!control_) {
    spdlog::error("river_control_v1 not advertised");
  }

  if (!seat_) {
    spdlog::error("wl_seat not advertised");
  }

  box_.set_name("tags");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);

  // Default to 9 tags, cap at 32
  const int num_tags =
      config["num-tags"].isUInt() ? std::min<int>(32, config_["num-tags"].asUInt()) : 9;

  const auto tag_labels = config["tag-labels"];
  const auto set_tags = config["set-tags"];
  const auto toggle_tags = config["toggle-tags"];
  for (int tag = 0; tag < num_tags; ++tag) {
    if (tag_labels.isArray() && !tag_labels.empty()) {
      buttons_.emplace_back(tag_labels[tag].asString());
    } else {
      // default name is the tag value
      buttons_.emplace_back(std::to_string(tag + 1));
    }

    auto &button = buttons_[tag];
    button.set_relief(Gtk::RELIEF_NONE);
    box_.pack_start(button, false, false, 0);

    if (!config_["disable-click"].asBool()) {
      if (set_tags.isArray() && !set_tags.empty())
        button.signal_clicked().connect(sigc::bind(
            sigc::mem_fun(*this, &Tags::handle_primary_clicked), set_tags[tag].asUInt()));
      else
        button.signal_clicked().connect(
            sigc::bind(sigc::mem_fun(*this, &Tags::handle_primary_clicked), (1 << tag)));
      if (toggle_tags.isArray() && !toggle_tags.empty())
        button.signal_button_press_event().connect(sigc::bind(
            sigc::mem_fun(*this, &Tags::handle_button_press), toggle_tags[tag].asUInt()));
      else
        button.signal_button_press_event().connect(
            sigc::bind(sigc::mem_fun(*this, &Tags::handle_button_press), (1 << tag)));
    }
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

  if (control_) {
    zriver_control_v1_destroy(control_);
  }
}

void Tags::handle_primary_clicked(uint32_t tag) {
  // Send river command to select tag on left mouse click
  zriver_command_callback_v1 *callback;
  zriver_control_v1_add_argument(control_, "set-focused-tags");
  zriver_control_v1_add_argument(control_, std::to_string(tag).c_str());
  callback = zriver_control_v1_run_command(control_, seat_);
  zriver_command_callback_v1_add_listener(callback, &command_callback_listener_impl, nullptr);
}

bool Tags::handle_button_press(GdkEventButton *event_button, uint32_t tag) {
  if (event_button->type == GDK_BUTTON_PRESS && event_button->button == 3) {
    // Send river command to toggle tag on right mouse click
    zriver_command_callback_v1 *callback;
    zriver_control_v1_add_argument(control_, "toggle-focused-tags");
    zriver_control_v1_add_argument(control_, std::to_string(tag).c_str());
    callback = zriver_control_v1_run_command(control_, seat_);
    zriver_command_callback_v1_add_listener(callback, &command_callback_listener_impl, nullptr);
  }
  return true;
}

void Tags::handle_focused_tags(uint32_t tags) {
  for (size_t i = 0; i < buttons_.size(); ++i) {
    if ((1 << i) & tags) {
      buttons_[i].get_style_context()->add_class("focused");
    } else {
      buttons_[i].get_style_context()->remove_class("focused");
    }
  }
}

void Tags::handle_view_tags(struct wl_array *view_tags) {
  uint32_t tags = 0;
  auto view_tag = reinterpret_cast<uint32_t *>(view_tags->data);
  auto end = view_tag + (view_tags->size / sizeof(uint32_t));
  for (; view_tag < end; ++view_tag) {
    tags |= *view_tag;
  }
  for (size_t i = 0; i < buttons_.size(); ++i) {
    if ((1 << i) & tags) {
      buttons_[i].get_style_context()->add_class("occupied");
    } else {
      buttons_[i].get_style_context()->remove_class("occupied");
    }
  }
}

void Tags::handle_urgent_tags(uint32_t tags) {
  for (size_t i = 0; i < buttons_.size(); ++i) {
    if ((1 << i) & tags) {
      buttons_[i].get_style_context()->add_class("urgent");
    } else {
      buttons_[i].get_style_context()->remove_class("urgent");
    }
  }
}

} /* namespace waybar::modules::river */
