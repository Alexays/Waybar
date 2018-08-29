#pragma once

#include <unistd.h>
#include <wordexp.h>
#include <fmt/format.h>
#include <gdk/gdk.h>
#include <wayland-client.h>
#include <gdk/gdkwayland.h>
#include "bar.hpp"

namespace waybar {

class Client {
  public:
    Client(int argc, char *argv[]);
    int main(int argc, char *argv[]);

    Glib::RefPtr<Gtk::Application> gtk_app;
    std::string css_file;
    std::string config_file;
    Glib::RefPtr<Gdk::Display> gdk_display;
    struct wl_display *wl_display = nullptr;
    struct wl_registry *registry = nullptr;
    struct zwlr_layer_shell_v1 *layer_shell = nullptr;
    struct zxdg_output_manager_v1 *xdg_output_manager = nullptr;
    struct wl_seat *seat = nullptr;
    std::vector<std::unique_ptr<Bar>> bars;

  private:
    void bindInterfaces();
    auto setupCss();

    static void handleGlobal(void *data, struct wl_registry *registry,
      uint32_t name, const char *interface, uint32_t version);
    static void handleGlobalRemove(void *data,
      struct wl_registry *registry, uint32_t name);
};

}
