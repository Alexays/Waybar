#include "modules/kbdlayout.hpp"

waybar::modules::KbdLayout::KbdLayout(const Json::Value &config)
  : ALabel(config, "{layout}"), current_group_(0) {
  label_.set_name("kbdlayout");
  enum xkb_context_flags ctx_flags = static_cast<enum xkb_context_flags>(XKB_CONTEXT_NO_DEFAULT_INCLUDES | XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
  xkb_ctx_ = xkb_context_new(ctx_flags);
}

waybar::modules::KbdLayout::~KbdLayout() {
  xkb_context_unref(xkb_ctx_);
}

void waybar::modules::KbdLayout::handleSeat(struct wl_seat* seat, uint32_t caps) {
  static const struct wl_keyboard_listener kbd_listener = {
                                                           kbdKeymap,
                                                           kbdEnter,
                                                           kbdLeave,
                                                           kbdKey,
                                                           kbdModifiers,
                                                           kbdRepeatInfo
  };

  if (!wl_kbd_ && (caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
    wl_kbd_ = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(wl_kbd_, &kbd_listener, this);
  } else if (wl_kbd_ && !(caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
    wl_keyboard_destroy(wl_kbd_);
    wl_kbd_ = NULL;
    xkb_keymap_unref(keymap_);
    keymap_ = NULL;
  }
}

auto waybar::modules::KbdLayout::update() -> void
{
  auto format = format_;

  xkb_keycode_t keycode = 38;
  xkb_layout_index_t layout = xkb_state_key_get_layout(xkb_state_, keycode);
  layout_ = xkb_keymap_layout_get_name(keymap_, layout);

  label_.set_label(fmt::format(format, fmt::arg("layout", layout_)));
}

void waybar::modules::KbdLayout::kbdKeymap(void *data, struct wl_keyboard *wl_kbd, uint32_t format, int fd, uint32_t size) {
  auto o = static_cast<waybar::modules::KbdLayout *>(data);
    void *buf;

    buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED) {
        printf("Failed to mmap keymap: %d\n", errno);
        close(fd);
        return;
    }

    o->keymap_ = xkb_keymap_new_from_buffer(o->xkb_ctx_, static_cast<const char*>(buf), size - 1,
                                            XKB_KEYMAP_FORMAT_TEXT_V1, static_cast<enum xkb_keymap_compile_flags>(0));
    munmap(buf, size);
    close(fd);
    if (!o->keymap_) {
        printf("Failed to compile keymap!\n");
        return;
    }

    o->xkb_state_ = xkb_state_new(o->keymap_);
    if (!o->xkb_state_) {
      printf("Failed to create XKB state!\n");
      return;
    }

    o->dp.emit();
}

void waybar::modules::KbdLayout::kbdModifiers(void *data, struct wl_keyboard *wl_kbd, uint32_t serial,
                                              uint32_t mods_depressed, uint32_t mods_latched,
                                              uint32_t mods_locked, uint32_t group) {

  auto o = static_cast<waybar::modules::KbdLayout *>(data);

  xkb_state_update_mask(o->xkb_state_, mods_depressed, mods_latched,
                        mods_locked, 0, 0, group);
  if (o->current_group_ != group) {
    o->current_group_ = group;
    o->dp.emit();
  }
}
