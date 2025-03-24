#include "modules/dwl/tags.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>
#include <wayland-client.h>

#include <algorithm>

#include "client.hpp"
#include "dwl-ipc-unstable-v2-client-protocol.h"

#define TAG_INACTIVE 0
#define TAG_ACTIVE 1
#define TAG_URGENT 2

namespace waybar::modules::dwl {

/* dwl stuff */
wl_array tags, layouts;

static uint32_t num_tags = 0;
static uint32_t num_counts = 0;
std::vector<std::string> tag_labels;

static void toggle_visibility(void *data, zdwl_ipc_output_v2 *zdwl_output_v2) {
  // Intentionally empty
}

static void active(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, uint32_t active) {
  // Intentionally empty
}

static void set_tag(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, uint32_t tag, uint32_t state,
                    uint32_t clients, uint32_t focused) {
  static_cast<Tags *>(data)->handle_view_tags(tag, state, clients, focused);

  num_tags = (state & ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE) ? num_tags | (1 << tag)
                                                           : num_tags & ~(1 << tag);
}

static void set_layout_symbol(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, const char *layout) {
  // Intentionally empty
}

static void title(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, const char *title) {
  // Intentionally empty
}

static void dwl_frame(void *data, zdwl_ipc_output_v2 *zdwl_output_v2) {
  // Intentionally empty
}

static void set_layout(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, uint32_t layout) {
  // Intentionally empty
}

static void appid(void *data, zdwl_ipc_output_v2 *zdwl_output_v2, const char *appid) {
  // Intentionally empty
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
    static_cast<Tags *>(data)->status_manager_ = static_cast<struct zdwl_ipc_manager_v2 *>(
        (zdwl_ipc_manager_v2 *)wl_registry_bind(registry, name, &zdwl_ipc_manager_v2_interface, 1));
  }
  if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    version = std::min<uint32_t>(version, 1);
    static_cast<Tags *>(data)->seat_ = static_cast<struct wl_seat *>(
        wl_registry_bind(registry, name, &wl_seat_interface, version));
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  /* Ignore event */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};
uint32_t Tags::get_label_position(std::string label) {
  uint32_t i;
  for (i = 0; i < num_counts; i++) {
    if (label == tag_labels[i]) {
      return i;
    }
  }
  return UINT32_MAX;
}

void Tags::add_button(uint32_t tag) {
  uint32_t inser_pos = 0;
  std::string label = tag_labels[tag];
  std::string added_label;
  uint32_t position = UINT32_MAX;

  for (auto &added_button : buttons_) {
    added_label = added_button.get_label();
    position = get_label_position(added_label);
    if (position != UINT32_MAX && position < (tag + 1)) {
      inser_pos++;
    }
  }

  buttons_.emplace(buttons_.begin() + (inser_pos), label);

  auto &button = buttons_[inser_pos];

  button.set_relief(Gtk::RELIEF_NONE);
  box_.pack_start(button, false, false, 0);
  for (size_t i = 0; i < buttons_.size(); ++i) {
    box_.reorder_child(buttons_[i], i);
  }

  if (!config_["disable-click"].asBool()) {
    button.signal_clicked().connect(
        sigc::bind(sigc::mem_fun(*this, &Tags::handle_primary_clicked), 1 << tag));
    button.signal_button_press_event().connect(
        sigc::bind(sigc::mem_fun(*this, &Tags::handle_button_press), 1 << tag));
  }
}

Tags::Tags(const std::string &id, const waybar::Bar &bar, const Json::Value &config)
    : waybar::AModule(config, "tags", id, false, false),
      status_manager_{nullptr},
      seat_{nullptr},
      bar_(bar),
      box_{bar.orientation, 0},
      hide_vacant_(false),
      output_status_{nullptr} {
  if (config_["hide-vacant"].asBool()) {
    hide_vacant_ = config_["hide-vacant"].asBool();
  }
  struct wl_display *display = Client::inst()->wl_display;
  struct wl_registry *registry = wl_display_get_registry(display);

  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

  if (!status_manager_) {
    spdlog::error("dwl_status_manager_v2 not advertised");
    return;
  }

  if (!seat_) {
    spdlog::error("wl_seat not advertised");
  }

  box_.set_name("tags");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  // Default to 9 tags, cap at 32
  num_counts =
      config["num-tags"].isUInt() ? std::min<uint32_t>(32, config_["num-tags"].asUInt()) : 9;

  tag_labels.resize(num_counts);

  for (uint32_t tag = 0; tag < num_counts; ++tag) {
    tag_labels[tag] = std::to_string(tag + 1);
  }
  const Json::Value custom_labels = config["tag-labels"];
  if (custom_labels.isArray() && !custom_labels.empty()) {
    for (uint32_t tag = 0; tag < std::min(num_counts, custom_labels.size()); ++tag) {
      tag_labels[tag] = custom_labels[tag].asString();
    }
  }

  if (!hide_vacant_) {
    uint32_t i = 1;
    while (i <= num_counts) {
      add_button(num_counts - i);
      i++;
    }
  }

  struct wl_output *output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
  output_status_ = zdwl_ipc_manager_v2_get_output(status_manager_, output);
  zdwl_ipc_output_v2_add_listener(output_status_, &output_status_listener_impl, this);

  zdwl_ipc_manager_v2_destroy(status_manager_);
  status_manager_ = nullptr;
}

Tags::~Tags() {
  if (output_status_) {
    zdwl_ipc_output_v2_destroy(output_status_);
  }
  if (status_manager_) {
    zdwl_ipc_manager_v2_destroy(status_manager_);
  }
}

void Tags::handle_primary_clicked(uint32_t tag) {
  if (!output_status_) return;

  zdwl_ipc_output_v2_set_tags(output_status_, tag, 1);
}

bool Tags::handle_button_press(GdkEventButton *event_button, uint32_t tag) {
  if (event_button->type == GDK_BUTTON_PRESS && event_button->button == 3) {
    if (!output_status_) return true;
    zdwl_ipc_output_v2_set_tags(output_status_, num_tags ^ tag, 0);
  }
  return true;
}

void Tags::handle_view_tags(uint32_t tag, uint32_t state, uint32_t clients, uint32_t focused) {
  bool is_new_button = true;
  std::string label;
  uint32_t position = UINT32_MAX;

  for (auto &added_button : buttons_) {
    label = added_button.get_label();
    position = get_label_position(label);
    if (position != UINT32_MAX && position == tag) {
      is_new_button = false;
    }
  }

  if (is_new_button && ((state & TAG_ACTIVE) || (state & TAG_URGENT) || clients)) {
    add_button(tag);
  }

  for (auto &button : buttons_) {
    label = button.get_label();
    position = get_label_position(label);
    if (position == UINT32_MAX || position != tag) continue;

    if (clients) {
      button.get_style_context()->add_class("occupied");
      button.set_visible(true);
    } else {
      button.get_style_context()->remove_class("occupied");
    }

    if (state & TAG_ACTIVE) {
      button.get_style_context()->add_class("focused");
      button.set_visible(true);
    } else {
      button.get_style_context()->remove_class("focused");
    }

    if (state & TAG_URGENT) {
      button.get_style_context()->add_class("urgent");
      button.set_visible(true);
    } else {
      button.get_style_context()->remove_class("urgent");
    }

    if (hide_vacant_ && !(state & TAG_ACTIVE) && !(state & TAG_URGENT) && !clients) {
      button.set_visible(false);
    } else {
      button.set_visible(true);
    }
  }
}

} /* namespace waybar::modules::dwl */
