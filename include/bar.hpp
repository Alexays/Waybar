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
    struct zwlr_layer_surface_v1 *layerSurface;
    std::unique_ptr<struct wl_output *> output;
    bool visible = true;
    auto setWidth(int) -> void;
    auto toggle() -> void;
  private:
    auto _setupWidgets() -> void;
    auto _setupCss() -> void;
    int _width = 10;
    Glib::RefPtr<Gtk::StyleContext> _styleContext;
    Glib::RefPtr<Gtk::CssProvider> _cssProvider;
  };

}
