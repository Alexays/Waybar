#pragma once

#include <gtkmm.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

namespace waybar {

  struct Client;

  struct Bar {
    Bar(Client& client, std::unique_ptr<struct wl_output *>&& output);
    Bar(const Bar&) = delete;
    Client& client;
    Gtk::Window window;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    std::unique_ptr<struct wl_output *> output;
    bool visible = true;
    auto set_width(int) -> void;
    auto toggle() -> void;
  private:
    auto setup_widgets() -> void;
    auto setup_css() -> void;

    int width = 10;
    Glib::RefPtr<Gtk::StyleContext> style_context;
    Glib::RefPtr<Gtk::CssProvider> css_provider;
  };

}
