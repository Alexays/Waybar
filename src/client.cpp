#include "client.hpp"

waybar::Client::Client(int argc, char* argv[])
  : gtk_main(argc, argv), gdk_display(Gdk::Display::get_default()),
    wl_display(gdk_wayland_display_get_wl_display(gdk_display->gobj()))
{
  auto getFirstValidPath = [] (std::vector<std::string> possiblePaths) {
    wordexp_t p;

    for (const std::string &path: possiblePaths) {
      if (wordexp(path.c_str(), &p, 0) == 0) {
        if (access(*p.we_wordv, F_OK) == 0) {
          std::string result = *p.we_wordv;
          wordfree(&p);
          return result;
        }
        wordfree(&p);
      }
    }

    return std::string();
  };

  config_file = getFirstValidPath({
    "$XDG_CONFIG_HOME/waybar/config",
    "$HOME/waybar/config",
    "/etc/xdg/waybar/config",
    "./resources/config",
  });
  css_file = getFirstValidPath({
    "$XDG_CONFIG_HOME/waybar/style.css",
    "$HOME/waybar/style.css",
    "/etc/xdg/waybar/style.css",
    "./resources/style.css",
  });

}

void waybar::Client::handleGlobal(void *data, struct wl_registry *registry,
  uint32_t name, const char *interface, uint32_t version)
{
  auto o = static_cast<waybar::Client *>(data);
  if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    o->layer_shell = static_cast<struct zwlr_layer_shell_v1 *>(
      wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version));
  } else if (strcmp(interface, wl_output_interface.name) == 0) {
    auto output = std::make_unique<struct wl_output *>();
    *output = static_cast<struct wl_output *>(wl_registry_bind(registry, name,
      &wl_output_interface, version));
    if (o->xdg_output_manager != nullptr) {
      o->bars.emplace_back(std::make_unique<Bar>(*o, std::move(output)));
    }
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    o->seat = static_cast<struct wl_seat *>(wl_registry_bind(registry, name,
      &wl_seat_interface, version));
  } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0
    && version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
      o->xdg_output_manager = static_cast<struct zxdg_output_manager_v1 *>(
        wl_registry_bind(registry, name,
        &zxdg_output_manager_v1_interface, ZXDG_OUTPUT_V1_NAME_SINCE_VERSION));
  }
}

void waybar::Client::handleGlobalRemove(void* /*data*/,
  struct wl_registry* /*registry*/, uint32_t /*name*/)
{
    // TODO(someone)
}

void waybar::Client::bindInterfaces()
{
  registry = wl_display_get_registry(wl_display);
  static const struct wl_registry_listener registry_listener = {
    .global = handleGlobal,
    .global_remove = handleGlobalRemove,
  };
  wl_registry_add_listener(registry, &registry_listener, this);
  wl_display_roundtrip(wl_display);
}

int waybar::Client::main(int /*argc*/, char* /*argv*/[])
{
  bindInterfaces();
  gtk_main.run();
  return 0;
}
