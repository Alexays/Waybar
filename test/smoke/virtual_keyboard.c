// Keyboard for compositor.sh: the headless backend has no input devices, so sway lists no keyboard
// and has no layout to report. This one carries a keymap with the layouts given in LAYOUTS
// (comma-separated, e.g. "us,fr") and stays until killed.
//
// Usage: virtual_keyboard LAYOUTS
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "virtual-keyboard-unstable-v1-client-protocol.h"

static struct wl_seat* seat;
static struct zwp_virtual_keyboard_manager_v1* manager;

static void global(void* data, struct wl_registry* registry, uint32_t name, const char* interface,
                   uint32_t version) {
  if (strcmp(interface, wl_seat_interface.name) == 0 && seat == NULL) {
    seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
  } else if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
    manager = wl_registry_bind(registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
  }
}

static void global_remove(void* data, struct wl_registry* registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {global, global_remove};

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s LAYOUTS\n", argv[0]);
    return 2;
  }
  struct xkb_context* ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  struct xkb_rule_names names = {.layout = argv[1]};
  struct xkb_keymap* keymap = xkb_keymap_new_from_names(ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (keymap == NULL) {
    fprintf(stderr, "cannot compile a keymap for '%s'\n", argv[1]);
    return 1;
  }
  char* text = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
  size_t size = strlen(text) + 1;
  int fd = memfd_create("keymap", MFD_CLOEXEC);
  if (fd < 0 || write(fd, text, size) != (ssize_t)size) {
    perror("keymap memfd");
    return 1;
  }

  struct wl_display* display = wl_display_connect(NULL);
  if (display == NULL) {
    fprintf(stderr, "cannot connect to the Wayland display\n");
    return 1;
  }
  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display);
  if (seat == NULL || manager == NULL) {
    fprintf(stderr, "compositor lacks a seat or zwp_virtual_keyboard_manager_v1\n");
    return 1;
  }

  struct zwp_virtual_keyboard_v1* keyboard =
      zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(manager, seat);
  zwp_virtual_keyboard_v1_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, size);
  wl_display_roundtrip(display);
  close(fd);
  printf("ready\n");
  fflush(stdout);

  while (wl_display_dispatch(display) != -1) {
  }
  return 0;
}
