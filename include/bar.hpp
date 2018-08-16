#pragma once

#include <json/json.h>
#include <gtkmm.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace waybar {

class Client;

class Bar {
  public:
    Bar(Client&, std::unique_ptr<struct wl_output *>&&);
    Bar(const Bar&) = delete;

    auto setWidth(uint32_t) -> void;
    auto toggle() -> void;

    Client& client;
    Gtk::Window window;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    std::unique_ptr<struct wl_output *> output;
    std::string outputName;
    bool visible = true;
  private:
    static void handleLogicalPosition(void *, struct zxdg_output_v1 *, int32_t,
      int32_t);
    static void handleLogicalSize(void *, struct zxdg_output_v1 *, int32_t,
      int32_t);
    static void handleDone(void *, struct zxdg_output_v1 *);
    static void handleName(void *, struct zxdg_output_v1 *, const char *);
    static void handleDescription(void *, struct zxdg_output_v1 *,
      const char *);
    static void layerSurfaceHandleConfigure(void *,
      struct zwlr_layer_surface_v1 *, uint32_t, uint32_t, uint32_t);
    static void layerSurfaceHandleClosed(void *,
      struct zwlr_layer_surface_v1 *);

    auto setupConfig() -> void;
    auto setupWidgets() -> void;
    auto setupCss() -> void;

    uint32_t width_ = 0;
    uint32_t height_ = 30;
    Json::Value config_;
    Glib::RefPtr<Gtk::StyleContext> style_context_;
    Glib::RefPtr<Gtk::CssProvider> css_provider_;
    struct zxdg_output_v1 *xdg_output_;
};

} // namespace waybar
