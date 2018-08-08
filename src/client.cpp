#include "client.hpp"

static void handle_global(void *data, struct wl_registry *registry,
  uint32_t name, const char *interface, uint32_t version)
{
  auto o = reinterpret_cast<waybar::Client *>(data);
  if (!strcmp(interface, zwlr_layer_shell_v1_interface.name)) {
    o->layer_shell = (zwlr_layer_shell_v1 *)wl_registry_bind(registry, name,
      &zwlr_layer_shell_v1_interface, version);
  } else if (!strcmp(interface, wl_output_interface.name)) {
    auto output = std::make_unique<struct wl_output *>();
    *output = (struct wl_output *)wl_registry_bind(registry, name,
      &wl_output_interface, version);
    o->bars.emplace_back(*o, std::move(output));
  } else if (!strcmp(interface, org_kde_kwin_idle_interface.name)) {
    o->idle_manager = (org_kde_kwin_idle *)wl_registry_bind(registry, name,
      &org_kde_kwin_idle_interface, version);
  } else if (!strcmp(interface, wl_seat_interface.name)) {
    o->seat = (struct wl_seat *)wl_registry_bind(registry, name,
      &wl_seat_interface, version);
  }
}

static void handle_global_remove(void *data,
  struct wl_registry *registry, uint32_t name)
{
    // TODO
}

static const struct wl_registry_listener registry_listener = {
  .global = handle_global,
  .global_remove = handle_global_remove,
};

waybar::Client::Client(int argc, char* argv[])
  : gtk_main(argc, argv),
    gdk_display(Gdk::Display::get_default()),
    wlDisplay(gdk_wayland_display_get_wl_display(gdk_display->gobj()))
{}

void waybar::Client::bind_interfaces()
{
  registry = wl_display_get_registry(wlDisplay);
  wl_registry_add_listener(registry, &registry_listener, this);
  wl_display_roundtrip(wlDisplay);
}

int waybar::Client::main(int argc, char* argv[])
{
  bind_interfaces();
  gtk_main.run();
  return 0;
}
