#pragma once

#include <fmt/format.h>
#include <algorithm>
#include "ALabel.hpp"
#include <xkbcommon/xkbcommon.h>
#include <wayland-client.h>
#include <sys/mman.h>

namespace waybar::modules {

class KbdLayout : public ALabel {
  public:
    KbdLayout(const Json::Value&);
    ~KbdLayout();
    auto update() -> void;
  void handleSeat(struct wl_seat* seat, uint32_t caps);
  struct wl_keyboard *wl_kbd_ = nullptr;
  struct xkb_context *xkb_ctx_ = nullptr;
  struct xkb_keymap *keymap_ = nullptr;
  struct xkb_state *xkb_state_ = nullptr;
  uint32_t current_group_ = 0;

private:

static void kbdKeymap(void *data, struct wl_keyboard *wl_kbd, uint32_t format,
                       int fd, uint32_t size);
static void kbdEnter(void *data, struct wl_keyboard *wl_kbd, uint32_t serial,
          struct wl_surface *surf, struct wl_array *keys) {}
static void kbdLeave(void *data, struct wl_keyboard *wl_kbd, uint32_t serial, struct wl_surface *surf) {}

static void kbdKey(void *data, struct wl_keyboard *wl_kbd, uint32_t serial, uint32_t time,
        uint32_t key, uint32_t state) {}

static void kbdModifiers(void *data, struct wl_keyboard *wl_kbd, uint32_t serial,
                         uint32_t mods_depressed, uint32_t mods_latched,
                         uint32_t mods_locked, uint32_t group);

static void kbdRepeatInfo(void *data, struct wl_keyboard *wl_kbd, int32_t rate,
                int32_t delay) {}
  std::string layout_ = "EN";
};

}  // namespace waybar::modules
