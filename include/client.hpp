#pragma once

#include <unistd.h>
#include <wordexp.h>

#include <iostream>

#include <fmt/format.h>

#include <gdk/gdk.h>
#include <gtkmm.h>
#include <wayland-client.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "idle-client-protocol.h"

#include "util/ptr_vec.hpp"

#include <gdk/gdkwayland.h>

#include "bar.hpp"

namespace waybar {

  struct Client {
    uint32_t height = 30;
    std::string cssFile;
    std::string configFile;

    Gtk::Main gtk_main;

    Glib::RefPtr<Gdk::Display> gdk_display;
    struct wl_display *wlDisplay;
    struct wl_registry *registry;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct org_kde_kwin_idle *idle_manager;
    struct wl_seat *seat;
    util::ptr_vec<Bar> bars;

    struct {
      sigc::signal<void(int, int)> workspace_state;
      sigc::signal<void(std::string)> focused_window_name;
    } signals;

    Client(int argc, char* argv[]);
    void bind_interfaces();
    auto setup_css();
    int main(int argc, char* argv[]);
  };
}
