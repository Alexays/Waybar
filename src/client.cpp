#include "client.hpp"

#include <spdlog/spdlog.h>

#include <iostream>

#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "util/clara.hpp"
#include "util/format.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

waybar::Client *waybar::Client::inst() {
  static auto c = new Client();
  return c;
}

void waybar::Client::handleGlobal(void *data, struct wl_registry *registry, uint32_t name,
                                  const char *interface, uint32_t version) {
  auto client = static_cast<Client *>(data);
  if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    // limit version to a highest supported by the client protocol file
    version = std::min<uint32_t>(version, zwlr_layer_shell_v1_interface.version);
    client->layer_shell = static_cast<struct zwlr_layer_shell_v1 *>(
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version));
  } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0 &&
             version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
    client->xdg_output_manager = static_cast<struct zxdg_output_manager_v1 *>(wl_registry_bind(
        registry, name, &zxdg_output_manager_v1_interface, ZXDG_OUTPUT_V1_NAME_SINCE_VERSION));
  } else if (strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) == 0) {
    client->idle_inhibit_manager = static_cast<struct zwp_idle_inhibit_manager_v1 *>(
        wl_registry_bind(registry, name, &zwp_idle_inhibit_manager_v1_interface, 1));
  }
}

void waybar::Client::handleGlobalRemove(void *data, struct wl_registry * /*registry*/,
                                        uint32_t name) {
  // Nothing here
}

void waybar::Client::handleOutput(struct waybar_output &output) {
  static const struct zxdg_output_v1_listener xdgOutputListener = {
      .logical_position = [](void *, struct zxdg_output_v1 *, int32_t, int32_t) {},
      .logical_size = [](void *, struct zxdg_output_v1 *, int32_t, int32_t) {},
      .done = &handleOutputDone,
      .name = &handleOutputName,
      .description = &handleOutputDescription,
  };
  // owned by output->monitor; no need to destroy
  auto wl_output = gdk_wayland_monitor_get_wl_output(output.monitor->gobj());
  output.xdg_output.reset(zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, wl_output));
  zxdg_output_v1_add_listener(output.xdg_output.get(), &xdgOutputListener, &output);
}

struct waybar::waybar_output &waybar::Client::getOutput(void *addr) {
  auto it = std::find_if(outputs_.begin(), outputs_.end(),
                         [&addr](const auto &output) { return &output == addr; });
  if (it == outputs_.end()) {
    throw std::runtime_error("Unable to find valid output");
  }
  return *it;
}

std::vector<Json::Value> waybar::Client::getOutputConfigs(struct waybar_output &output) {
  return config.getOutputConfigs(output.name, output.identifier);
}

void waybar::Client::handleOutputDone(void *data, struct zxdg_output_v1 * /*xdg_output*/) {
  auto client = waybar::Client::inst();
  try {
    auto &output = client->getOutput(data);
    /**
     * Multiple .done events may arrive in batch. In this case libwayland would queue
     * xdg_output.destroy and dispatch all pending events, triggering this callback several times
     * for the same output. .done events can also arrive after that for a scale or position changes.
     * We wouldn't want to draw a duplicate bar for each such event either.
     *
     * All the properties we care about are immutable so it's safe to delete the xdg_output object
     * on the first event and use the ptr value to check that the callback was already invoked.
     */
    if (output.xdg_output) {
      output.xdg_output.reset();
      spdlog::debug("Output detection done: {} ({})", output.name, output.identifier);

      auto configs = client->getOutputConfigs(output);
      if (!configs.empty()) {
        for (const auto &config : configs) {
          client->bars.emplace_back(std::make_unique<Bar>(&output, config));
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

void waybar::Client::handleOutputName(void *data, struct zxdg_output_v1 * /*xdg_output*/,
                                      const char *name) {
  auto client = waybar::Client::inst();
  try {
    auto &output = client->getOutput(data);
    output.name = name;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

void waybar::Client::handleOutputDescription(void *data, struct zxdg_output_v1 * /*xdg_output*/,
                                             const char *description) {
  auto client = waybar::Client::inst();
  try {
    auto &output = client->getOutput(data);
    const char *open_paren = strrchr(description, '(');

    // Description format: "identifier (name)"
    size_t identifier_length = open_paren - description;
    output.identifier = std::string(description, identifier_length - 1);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

void waybar::Client::handleMonitorAdded(Glib::RefPtr<Gdk::Monitor> monitor) {
  auto &output = outputs_.emplace_back();
  output.monitor = monitor;
  handleOutput(output);
}

void waybar::Client::handleMonitorRemoved(Glib::RefPtr<Gdk::Monitor> monitor) {
  spdlog::debug("Output removed: {} {}", monitor->get_manufacturer(), monitor->get_model());
  /* This event can be triggered from wl_display_roundtrip called by GTK or our code.
   * Defer destruction of bars for the output to the next iteration of the event loop to avoid
   * deleting objects referenced by currently executed code.
   */
  Glib::signal_idle().connect_once(
      sigc::bind(sigc::mem_fun(*this, &Client::handleDeferredMonitorRemoval), monitor),
      Glib::PRIORITY_HIGH_IDLE);
}

void waybar::Client::handleDeferredMonitorRemoval(Glib::RefPtr<Gdk::Monitor> monitor) {
  for (auto it = bars.begin(); it != bars.end();) {
    if ((*it)->output->monitor == monitor) {
      auto output_name = (*it)->output->name;
      (*it)->window.hide();
      gtk_app->remove_window((*it)->window);
      it = bars.erase(it);
      spdlog::info("Bar removed from output: {}", output_name);
    } else {
      ++it;
    }
  }
  outputs_.remove_if([&monitor](const auto &output) { return output.monitor == monitor; });
}

const std::string waybar::Client::getStyle(const std::string &style) {
  auto css_file = style.empty() ? Config::findConfigPath({"style.css"}) : style;
  if (!css_file) {
    throw std::runtime_error("Missing required resource files");
  }
  spdlog::info("Using CSS file {}", css_file.value());
  return css_file.value();
};

auto waybar::Client::setupCss(const std::string &css_file) -> void {
  css_provider_ = Gtk::CssProvider::create();
  style_context_ = Gtk::StyleContext::create();

  // Load our css file, wherever that may be hiding
  if (!css_provider_->load_from_path(css_file)) {
    throw std::runtime_error("Can't open style file");
  }
  // there's always only one screen
  style_context_->add_provider_for_screen(Gdk::Screen::get_default(), css_provider_,
                                          GTK_STYLE_PROVIDER_PRIORITY_USER);
}

void waybar::Client::bindInterfaces() {
  registry = wl_display_get_registry(wl_display);
  static const struct wl_registry_listener registry_listener = {
      .global = handleGlobal,
      .global_remove = handleGlobalRemove,
  };
  wl_registry_add_listener(registry, &registry_listener, this);
  wl_display_roundtrip(wl_display);
  if (layer_shell == nullptr || xdg_output_manager == nullptr) {
    throw std::runtime_error("Failed to acquire required resources.");
  }
  // add existing outputs and subscribe to updates
  for (auto i = 0; i < gdk_display->get_n_monitors(); ++i) {
    auto monitor = gdk_display->get_monitor(i);
    handleMonitorAdded(monitor);
  }
  gdk_display->signal_monitor_added().connect(sigc::mem_fun(*this, &Client::handleMonitorAdded));
  gdk_display->signal_monitor_removed().connect(
      sigc::mem_fun(*this, &Client::handleMonitorRemoved));
}

int waybar::Client::main(int argc, char *argv[]) {
  bool show_help = false;
  bool show_version = false;
  std::string config_opt;
  std::string style_opt;
  std::string log_level;
  auto cli = clara::detail::Help(show_help) |
             clara::detail::Opt(show_version)["-v"]["--version"]("Show version") |
             clara::detail::Opt(config_opt, "config")["-c"]["--config"]("Config path") |
             clara::detail::Opt(style_opt, "style")["-s"]["--style"]("Style path") |
             clara::detail::Opt(
                 log_level,
                 "trace|debug|info|warning|error|critical|off")["-l"]["--log-level"]("Log level") |
             clara::detail::Opt(bar_id, "id")["-b"]["--bar"]("Bar id");
  auto res = cli.parse(clara::detail::Args(argc, argv));
  if (!res) {
    spdlog::error("Error in command line: {}", res.errorMessage());
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
  if (!log_level.empty()) {
    spdlog::set_level(spdlog::level::from_str(log_level));
  }
  gtk_app = Gtk::Application::create(argc, argv, "fr.arouillard.waybar",
                                     Gio::APPLICATION_HANDLES_COMMAND_LINE);
  gdk_display = Gdk::Display::get_default();
  if (!gdk_display) {
    throw std::runtime_error("Can't find display");
  }
  if (!GDK_IS_WAYLAND_DISPLAY(gdk_display->gobj())) {
    throw std::runtime_error("Bar need to run under Wayland");
  }
  wl_display = gdk_wayland_display_get_wl_display(gdk_display->gobj());
  config.load(config_opt);
  auto css_file = getStyle(style_opt);
  setupCss(css_file);
  bindInterfaces();
  gtk_app->hold();
  gtk_app->run();
  bars.clear();
  return 0;
}

void waybar::Client::reset() { gtk_app->quit(); }
