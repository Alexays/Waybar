#pragma once

#include <json/json.h>
#include <gtkmm.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

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
    std::string outputName;
    auto setWidth(uint32_t) -> void;
    auto toggle() -> void;
  private:
    static void _handleLogicalPosition(void *data,
      struct zxdg_output_v1 *zxdg_output_v1, int32_t x, int32_t y);
    static void _handleLogicalSize(void *data,
      struct zxdg_output_v1 *zxdg_output_v1, int32_t width, int32_t height);
    static void _handleDone(void *data, struct zxdg_output_v1 *zxdg_output_v1);
    static void _handleName(void *data, struct zxdg_output_v1 *xdg_output,
      const char *name);
    static void _handleDescription(void *data,
      struct zxdg_output_v1 *zxdg_output_v1, const char *description);
    static void _layerSurfaceHandleConfigure(void *data,
      struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width,
      uint32_t height);
    static void _layerSurfaceHandleClosed(void *data,
      struct zwlr_layer_surface_v1 *surface);
    auto _setupConfig() -> void;
    auto _setupWidgets() -> void;
    auto _setupCss() -> void;
    uint32_t _width = 0;
    uint32_t _height = 30;
    Json::Value _config;
    Glib::RefPtr<Gtk::StyleContext> _styleContext;
    Glib::RefPtr<Gtk::CssProvider> _cssProvider;
    struct zxdg_output_v1 *_xdgOutput;
  };

}
