// Pointer input for interact.sh: the headless backend has no input devices, so the seat has no
// pointer and `swaymsg seat cursor` events never reach clients. A virtual pointer gives it one.
//
// Usage: pointer_click X Y OUTPUT_WIDTH OUTPUT_HEIGHT [BUTTON] [HOLD_MS]
// Moves to (X, Y), clicks BUTTON (evdev code, default BTN_LEFT), then keeps the device for
// HOLD_MS: destroying it removes the seat's pointer, which counts as the pointer leaving.
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

static struct zwlr_virtual_pointer_manager_v1* manager;

static void global(void* data, struct wl_registry* registry, uint32_t name, const char* interface,
                   uint32_t version) {
  if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
    manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
  }
}

static void global_remove(void* data, struct wl_registry* registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {global, global_remove};

int main(int argc, char** argv) {
  if (argc < 5) {
    fprintf(stderr, "usage: %s X Y OUTPUT_WIDTH OUTPUT_HEIGHT [BUTTON] [HOLD_MS]\n", argv[0]);
    return 2;
  }
  uint32_t x = strtoul(argv[1], NULL, 0), y = strtoul(argv[2], NULL, 0);
  uint32_t w = strtoul(argv[3], NULL, 0), h = strtoul(argv[4], NULL, 0);
  uint32_t button = argc > 5 ? strtoul(argv[5], NULL, 0) : BTN_LEFT;
  useconds_t hold = argc > 6 ? strtoul(argv[6], NULL, 0) * 1000 : 200000;

  struct wl_display* display = wl_display_connect(NULL);
  if (display == NULL) {
    fprintf(stderr, "cannot connect to the Wayland display\n");
    return 1;
  }
  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display);
  if (manager == NULL) {
    fprintf(stderr, "compositor lacks zwlr_virtual_pointer_manager_v1\n");
    return 1;
  }

  struct zwlr_virtual_pointer_v1* pointer =
      zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, NULL);
  wl_display_roundtrip(display);
  // Clients bind wl_pointer only once the seat announces the new capability
  usleep(300000);

  zwlr_virtual_pointer_v1_motion_absolute(pointer, 0, x, y, w, h);
  zwlr_virtual_pointer_v1_frame(pointer);
  wl_display_roundtrip(display);
  usleep(200000);

  zwlr_virtual_pointer_v1_button(pointer, 0, button, WL_POINTER_BUTTON_STATE_PRESSED);
  zwlr_virtual_pointer_v1_frame(pointer);
  wl_display_roundtrip(display);
  usleep(50000);
  zwlr_virtual_pointer_v1_button(pointer, 0, button, WL_POINTER_BUTTON_STATE_RELEASED);
  zwlr_virtual_pointer_v1_frame(pointer);
  wl_display_roundtrip(display);
  usleep(hold);

  zwlr_virtual_pointer_v1_destroy(pointer);
  wl_display_roundtrip(display);
  wl_display_disconnect(display);
  return 0;
}
