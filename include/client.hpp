#pragma once

#include <unistd.h>
#include <wordexp.h>

#include <fmt/format.h>

#include <gdk/gdk.h>
#include <wayland-client.h>

#include <gdk/gdkwayland.h>

#include "bar.hpp"

namespace waybar {

  struct Client {
    std::string cssFile;
    std::string configFile;

    Gtk::Main gtk_main;

    Glib::RefPtr<Gdk::Display> gdk_display;
    struct wl_display *wlDisplay = nullptr;
    struct wl_registry *registry = nullptr;
    struct zwlr_layer_shell_v1 *layerShell = nullptr;
    struct zxdg_output_manager_v1 *xdgOutputManager = nullptr;
    struct wl_seat *seat = nullptr;
    std::vector<std::unique_ptr<Bar>> bars;

    Client(int argc, char* argv[]);
    void bind_interfaces();
    auto setup_css();
    int main(int argc, char* argv[]);
  private:
    static void _handle_global(void *data, struct wl_registry *registry,
      uint32_t name, const char *interface, uint32_t version);
    static void _handle_global_remove(void *data,
      struct wl_registry *registry, uint32_t name);
  };
}
