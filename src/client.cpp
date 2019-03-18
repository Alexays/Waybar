#include "client.hpp"
#include "util/clara.hpp"
#include <iostream>

waybar::Client::Client(int argc, char* argv[])
  : gtk_main(argc, argv),
    gdk_display(Gdk::Display::get_default())
{
  if (!gdk_display) {
    throw std::runtime_error("Can't find display");
  }
  if (!GDK_IS_WAYLAND_DISPLAY(gdk_display->gobj())) {
    throw std::runtime_error("Bar need to run under Wayland");
  }
  wl_display = gdk_wayland_display_get_wl_display(gdk_display->gobj());
}

const std::string waybar::Client::getValidPath(std::vector<std::string> paths)
{
  wordexp_t p;

  for (const std::string &path: paths) {
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
      o->bars.emplace_back(std::make_unique<Bar>(*o, std::move(output), name));
    }
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    o->seat = static_cast<struct wl_seat *>(wl_registry_bind(registry, name,
      &wl_seat_interface, version));
  } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0
    && version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
      o->xdg_output_manager = static_cast<struct zxdg_output_manager_v1 *>(
        wl_registry_bind(registry, name,
        &zxdg_output_manager_v1_interface, ZXDG_OUTPUT_V1_NAME_SINCE_VERSION));
  } else if (strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) == 0) {
    o->idle_inhibit_manager = static_cast<struct zwp_idle_inhibit_manager_v1 *>(
        wl_registry_bind(registry, name,
        &zwp_idle_inhibit_manager_v1_interface, 1));
  }
}

void waybar::Client::handleGlobalRemove(void* data,
  struct wl_registry* /*registry*/, uint32_t name)
{
  auto o = static_cast<waybar::Client *>(data);
  for (auto it = o->bars.begin(); it != o->bars.end(); ++it) {
    if ((*it)->wl_name == name) {
      auto output_name = (*it)->output_name;
      o->bars.erase(it);
      std::cout << "Bar removed from output: " + output_name << std::endl;
      break;
    }
  }
}

void waybar::Client::setupConfigs(const std::string& config, const std::string& style)
{
  config_file = config.empty() ? getValidPath({
    "$XDG_CONFIG_HOME/waybar/config",
    "$HOME/.config/waybar/config",
    "$HOME/waybar/config",
    "/etc/xdg/waybar/config",
    "./resources/config",
  }) : config;
  css_file = style.empty() ? getValidPath({
    "$XDG_CONFIG_HOME/waybar/style.css",
    "$HOME/.config/waybar/style.css",
    "$HOME/waybar/style.css",
    "/etc/xdg/waybar/style.css",
    "./resources/style.css",
  }) : style;
  if (css_file.empty() || config_file.empty()) {
    throw std::runtime_error("Missing required resources files");
  }
  std::cout << "Resources files: " + config_file + ", " + css_file << std::endl;
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
  if (!layer_shell || !seat || !xdg_output_manager) {
    throw std::runtime_error("Failed to acquire required resources.");
  }
  wl_display_roundtrip(wl_display);
}

int waybar::Client::main(int argc, char* argv[])
{
  bool show_help = false;
  bool show_version = false;
  std::string config;
  std::string style;
  std::string bar_id;
  auto cli = clara::detail::Help(show_help)
    | clara::detail::Opt(show_version)["-v"]["--version"]("Show version")
    | clara::detail::Opt(config, "config")["-c"]["--config"]("Config path")
    | clara::detail::Opt(style, "style")["-s"]["--style"]("Style path")
    | clara::detail::Opt(bar_id, "id")["-b"]["--bar"]("Bar id");
  auto res = cli.parse(clara::detail::Args(argc, argv));
  if (!res) {
    std::cerr << "Error in command line: " << res.errorMessage() << std::endl;
    return 1;
  }
  if (show_help) {
    std::cout << cli << std::endl;
    return 0;
  }
  if (show_version) {
    std::cout << "Waybar v" << VERSION << std::endl;
    return 0;
  }
  setupConfigs(config, style);
  bindInterfaces();
  gtk_main.run();
  bars.clear();
  zxdg_output_manager_v1_destroy(xdg_output_manager);
  zwlr_layer_shell_v1_destroy(layer_shell);
  zwp_idle_inhibit_manager_v1_destroy(idle_inhibit_manager);
  wl_registry_destroy(registry);
  wl_seat_destroy(seat);
  wl_display_disconnect(wl_display);
  return 0;
}
