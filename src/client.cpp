#include "client.hpp"

waybar::Client::Client(int argc, char* argv[])
  : gtk_main(argc, argv),
    gdk_display(Gdk::Display::get_default()),
    wlDisplay(gdk_wayland_display_get_wl_display(gdk_display->gobj()))
{
  auto getFirstValidPath = [] (std::vector<std::string> possiblePaths) {
    wordexp_t p;

    for (std::string path: possiblePaths) {
      if (wordexp(path.c_str(), &p, 0) == 0) {
        if (access(p.we_wordv[0], F_OK) == 0) {
          std::string result = p.we_wordv[0];
          wordfree(&p);
          return result;
        } else {
          wordfree(&p);
        }
      }
    }

    return std::string();
  };

  configFile = getFirstValidPath({
    "$XDG_CONFIG_HOME/waybar/config",
    "$HOME/waybar/config",
    "/etc/xdg/waybar/config",
    "./resources/config",
  });
  cssFile = getFirstValidPath({
    "$XDG_CONFIG_HOME/waybar/style.css",
    "$HOME/waybar/style.css",
    "/etc/xdg/waybar/style.css",
    "./resources/style.css",
  });

}

void waybar::Client::_handle_global(void *data, struct wl_registry *registry,
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
    o->bars.emplace_back(std::make_unique<Bar>(*o, std::move(output)));
  } else if (!strcmp(interface, wl_seat_interface.name)) {
    o->seat = (struct wl_seat *)wl_registry_bind(registry, name,
      &wl_seat_interface, version);
  } else if (!strcmp(interface, zxdg_output_manager_v1_interface.name)
    && version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
      o->xdg_output_manager =
        (struct zxdg_output_manager_v1 *)wl_registry_bind(registry, name,
        &zxdg_output_manager_v1_interface, ZXDG_OUTPUT_V1_NAME_SINCE_VERSION);
    }
}

void waybar::Client::_handle_global_remove(void *data,
  struct wl_registry *registry, uint32_t name)
{
    // TODO
}

void waybar::Client::bind_interfaces()
{
  registry = wl_display_get_registry(wlDisplay);
  static const struct wl_registry_listener registry_listener = {
    .global = _handle_global,
    .global_remove = _handle_global_remove,
  };
  wl_registry_add_listener(registry, &registry_listener, this);
  wl_display_roundtrip(wlDisplay);
}

int waybar::Client::main(int argc, char* argv[])
{
  bind_interfaces();
  gtk_main.run();
  return 0;
}
