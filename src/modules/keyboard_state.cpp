#include "modules/keyboard_state.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <utility>

#include "gdkmm/general.h"
#include "glibmm/error.h"
#include "glibmm/refptr.h"
#include "util/format.hpp"
#include "util/rewrite_string.hpp"
#include "util/string.hpp"

namespace waybar::modules {

static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format,
                            int32_t fd, uint32_t size) {
  static_cast<KeyboardState *>(data)->handle_keymap(format, fd, size);
}

static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
                           struct wl_surface *surface, struct wl_array *keys) {
  /* nop */
}

static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
                           struct wl_surface *surface) {
  /* nop */
}

static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
                         uint32_t time, uint32_t key, uint32_t _state) {
  /* nop */
}

static void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
                               uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked,
                               uint32_t group) {
  static_cast<KeyboardState *>(data)->handle_modifiers(mods_depressed, mods_latched, mods_locked,
                                                       group);
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate,
                                 int32_t delay) {
  /* nop */
}

static const struct wl_keyboard_listener keyboard_impl = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

static void seat_capabilities(void *data, struct wl_seat *wl_seat, unsigned int caps) {
  static_cast<KeyboardState *>(data)->handle_seat_capabilities(caps);
}

static void seat_name(void *data, struct wl_seat *wl_seat, const char *name) { /* nop */ }

const struct wl_seat_listener seat_impl = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
  if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    static_cast<KeyboardState *>(data)->register_seat(registry, name, version);
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
  /* nop */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

KeyboardState::KeyboardState(const std::string &id, const waybar::Bar &bar,
                             const Json::Value &config)
    : waybar::AModule(config, "keyboard-state", id, "{}"),
      box_(bar.orientation, 0),
      numlock_label_(""),
      capslock_label_(""),
      scrolllock_label_(""),
      numlock_format_(config_["format"].isString() ? config_["format"].asString()
                      : config_["format"]["numlock"].isString()
                          ? config_["format"]["numlock"].asString()
                          : "{name} {icon}"),
      capslock_format_(config_["format"].isString() ? config_["format"].asString()
                       : config_["format"]["capslock"].isString()
                           ? config_["format"]["capslock"].asString()
                           : "{name} {icon}"),
      scrolllock_format_(config_["format"].isString() ? config_["format"].asString()
                         : config_["format"]["scrolllock"].isString()
                             ? config_["format"]["scrolllock"].asString()
                             : "{name} {icon}"),
      icon_locked_(config_["format-icons"]["locked"].isString()
                       ? config_["format-icons"]["locked"].asString()
                       : "locked"),
      icon_unlocked_(config_["format-icons"]["unlocked"].isString()
                         ? config_["format-icons"]["unlocked"].asString()
                         : "unlocked"),
      seat_{nullptr},
      keyboard_{nullptr},
      xkb_context_{nullptr},
      xkb_state_{nullptr},
      xkb_keymap_{nullptr} {
  box_.set_name("keyboard-state");
  if (config_["numlock"].asBool()) {
    numlock_label_.get_style_context()->add_class("numlock");
    box_.pack_end(numlock_label_, false, false, 0);
  }
  if (config_["capslock"].asBool()) {
    capslock_label_.get_style_context()->add_class("capslock");
    box_.pack_end(capslock_label_, false, false, 0);
  }
  if (config_["scrolllock"].asBool()) {
    scrolllock_label_.get_style_context()->add_class("scrolllock");
    box_.pack_end(scrolllock_label_, false, false, 0);
  }
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  xkb_context_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

  struct wl_display *display = Client::inst()->wl_display;
  struct wl_registry *registry = wl_display_get_registry(display);

  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

  if (!seat_) {
    spdlog::error("Failed to get wayland seat");
    return;
  }
}

KeyboardState::~KeyboardState() {
  xkb_context_unref(xkb_context_);
}

void KeyboardState::update() {
  if (xkb_state_) {
    int numlock = xkb_state_led_name_is_active(xkb_state_, XKB_LED_NAME_NUM);
    update_led(&numlock_label_, numlock_format_, "Num", numlock);

    int capslock = xkb_state_led_name_is_active(xkb_state_, XKB_LED_NAME_CAPS);
    update_led(&capslock_label_, capslock_format_, "Caps", capslock);

    int scrolllock = xkb_state_led_name_is_active(xkb_state_, XKB_LED_NAME_SCROLL);
    update_led(&scrolllock_label_, scrolllock_format_, "Scroll", scrolllock);
  }

  AModule::update();
}

void KeyboardState::update_led(Gtk::Label *label, std::string format, std::string name,
                               bool locked) {
  std::string text =
      fmt::format(fmt::runtime(format), fmt::arg("icon", locked ? icon_locked_ : icon_unlocked_),
                  fmt::arg("name", name));
  label->set_markup(text);

  if (locked) {
    label->get_style_context()->add_class("locked");
  } else {
    label->get_style_context()->remove_class("locked");
  }
}

void KeyboardState::handle_keymap(uint32_t format, int32_t fd, uint32_t size) {
  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    spdlog::error("unknown keymap format {0}", format);
    return;
  }
  char *map_shm = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (map_shm == MAP_FAILED) {
    close(fd);
    spdlog::error("unable to initialize keymap shm");
    return;
  }

  xkb_keymap_unref(xkb_keymap_);
  xkb_state_unref(xkb_state_);

  xkb_keymap_ = xkb_keymap_new_from_string(xkb_context_, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1,
                                           XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(map_shm, size);
  close(fd);

  xkb_state_ = xkb_state_new(xkb_keymap_);
  dp.emit();
}

void KeyboardState::handle_modifiers(uint32_t mods_depressed, uint32_t mods_latched,
                                     uint32_t mods_locked, uint32_t group) {
  if (!xkb_state_) {
    return;
  }
  xkb_state_update_mask(xkb_state_, mods_depressed, mods_latched, mods_locked, 0, 0, group);
  dp.emit();
}

void KeyboardState::handle_seat_capabilities(unsigned int caps) {
  if (keyboard_) {
    wl_keyboard_release(keyboard_);
    keyboard_ = nullptr;
  }
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
    keyboard_ = wl_seat_get_keyboard(seat_);
    wl_keyboard_add_listener(keyboard_, &keyboard_impl, this);
  }
}

void KeyboardState::register_seat(struct wl_registry *registry, uint32_t name, uint32_t version) {
  if (seat_) {
    spdlog::warn("Register seat again although already existing!");
    return;
  }
  version = std::min<uint32_t>(version, wl_seat_interface.version);

  seat_ = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, version));
  wl_seat_add_listener(seat_, &seat_impl, this);
}

} /* namespace waybar::modules */
