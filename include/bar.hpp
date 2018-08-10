#pragma once

#include <json/json.h>
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
    auto setWidth(uint32_t) -> void;
    auto toggle() -> void;
  private:
    static void _handleGeometry(void *data, struct wl_output *wl_output,
      int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
      int32_t subpixel, const char *make, const char *model, int32_t transform);
    static void _handleMode(void *data, struct wl_output *wl_output,
      uint32_t f, int32_t w, int32_t h, int32_t refresh);
    static void _handleDone(void *data, struct wl_output *);
    static void _handleScale(void *data, struct wl_output *wl_output,
      int32_t factor);
    static void _layerSurfaceHandleConfigure(void *data,
      struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width,
      uint32_t height);
    static void _layerSurfaceHandleClosed(void *data,
      struct zwlr_layer_surface_v1 *surface);
    auto _setupConfig() -> void;
    auto _setupWidgets() -> void;
    auto _setupCss() -> void;
    uint32_t _width = 10;
    uint32_t _height = 30;
    Json::Value _config;
    Glib::RefPtr<Gtk::StyleContext> _styleContext;
    Glib::RefPtr<Gtk::CssProvider> _cssProvider;
  };

}
