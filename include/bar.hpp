#pragma once

#include <json/json.h>
#include <glibmm/refptr.h>
#include <gtkmm/main.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/window.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "IModule.hpp"

namespace waybar {

class Client;
class Factory;

class Bar {
  public:
    Bar(const Client&, std::unique_ptr<struct wl_output *>&&, uint32_t);
    Bar(const Bar&) = delete;
    ~Bar() = default;

    auto toggle() -> void;
    void handleSignal(int);

    const Client& client;
    Gtk::Window window;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    std::unique_ptr<struct wl_output *> output;
    std::string output_name;
    uint32_t wl_name;
    bool visible = true;
    bool vertical = false;
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

    void initBar();
    bool isValidOutput(const Json::Value &config);
    void destroyOutput();
    auto setupConfig() -> void;
    auto setupWidgets() -> void;
    auto setupCss() -> void;
    void getModules(const Factory&, const std::string&);

    uint32_t width_ = 0;
    uint32_t height_ = 30;
    Json::Value config_;
    Glib::RefPtr<Gtk::StyleContext> style_context_;
    Glib::RefPtr<Gtk::CssProvider> css_provider_;
    struct zxdg_output_v1 *xdg_output_;
    Gtk::Box left_;
    Gtk::Box center_;
    Gtk::Box right_;
    Gtk::Box box_;
    std::vector<std::unique_ptr<waybar::IModule>> modules_left_;
    std::vector<std::unique_ptr<waybar::IModule>> modules_center_;
    std::vector<std::unique_ptr<waybar::IModule>> modules_right_;
};

}
