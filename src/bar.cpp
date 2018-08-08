#include <condition_variable>
#include <gdk/gdkwayland.h>
#include <thread>
#include "bar.hpp"
#include "client.hpp"
#include "util/chrono.hpp"
#include "modules/clock.hpp"
#include "modules/workspaces.hpp"
#include "modules/battery.hpp"
#include "modules/memory.hpp"

static void handle_geometry(void *data, struct wl_output *wl_output, int32_t x,
  int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel,
  const char *make, const char *model, int32_t transform)
{
  // Nothing here
}

static void handle_mode(void *data, struct wl_output *wl_output, uint32_t f,
  int32_t w, int32_t h, int32_t refresh)
{
  auto o = reinterpret_cast<waybar::Bar *>(data);
  std::cout << fmt::format("Bar width configured: {}", w) << std::endl;
  o->set_width(w);
}

static void handle_done(void *data, struct wl_output *)
{
  // Nothing here
}

static void handle_scale(void *data, struct wl_output *wl_output,
  int32_t factor)
{
  // Nothing here
}

static const struct wl_output_listener outputListener = {
    .geometry = handle_geometry,
    .mode = handle_mode,
    .done = handle_done,
    .scale = handle_scale,
};

static void layer_surface_handle_configure(
  void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial,
  uint32_t width, uint32_t height)
{
  auto o = reinterpret_cast<waybar::Bar *>(data);
  o->window.show_all();
  zwlr_layer_surface_v1_ack_configure(surface, serial);
  if (o->client.height != height)
  {
    height = o->client.height;
    std::cout << fmt::format("New Height: {}", height) << std::endl;
    zwlr_layer_surface_v1_set_size(surface, width, height);
    zwlr_layer_surface_v1_set_exclusive_zone(surface, o->visible ? height : 0);
    wl_surface_commit(o->surface);
  }
}

static void layer_surface_handle_closed(void *data,
  struct zwlr_layer_surface_v1 *surface)
{
  auto o = reinterpret_cast<waybar::Bar *>(data);
  zwlr_layer_surface_v1_destroy(o->layer_surface);
  o->layer_surface = NULL;
  wl_surface_destroy(o->surface);
  o->surface = NULL;
  o->window.close();
}

static const struct zwlr_layer_surface_v1_listener layerSurfaceListener = {
    .configure = layer_surface_handle_configure,
    .closed = layer_surface_handle_closed,
};

waybar::Bar::Bar(Client &client, std::unique_ptr<struct wl_output *> &&p_output)
  : client(client), window{Gtk::WindowType::WINDOW_TOPLEVEL},
    output(std::move(p_output))
{
  wl_output_add_listener(*output, &outputListener, this);
  window.set_title("waybar");
  window.set_decorated(false);
  // window.set_resizable(false);
  setup_css();
  setup_widgets();
  gtk_widget_realize(GTK_WIDGET(window.gobj()));
  GdkWindow *gdkWindow = gtk_widget_get_window(GTK_WIDGET(window.gobj()));
  gdk_wayland_window_set_use_custom_surface(gdkWindow);
  surface = gdk_wayland_window_get_wl_surface(gdkWindow);
  layer_surface = zwlr_layer_shell_v1_get_layer_surface(
    client.layer_shell, surface, *output, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
    "waybar");
  zwlr_layer_surface_v1_set_anchor(layer_surface,
    ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
  zwlr_layer_surface_v1_set_size(layer_surface, width, client.height);
  zwlr_layer_surface_v1_add_listener(layer_surface, &layerSurfaceListener,
    this);
  wl_surface_commit(surface);
}

auto waybar::Bar::setup_css() -> void
{
  css_provider = Gtk::CssProvider::create();
  style_context = Gtk::StyleContext::create();

  // load our css file, wherever that may be hiding
  if (css_provider->load_from_path(client.css_file))
  {
    Glib::RefPtr<Gdk::Screen> screen = window.get_screen();
    style_context->add_provider_for_screen(screen, css_provider,
                                           GTK_STYLE_PROVIDER_PRIORITY_USER);
  }
}

auto waybar::Bar::set_width(int width) -> void
{
  this->width = width;
  window.set_size_request(width);
  window.resize(width, client.height);
  zwlr_layer_surface_v1_set_size(layer_surface, width, 40);
  wl_surface_commit(surface);
}

auto waybar::Bar::toggle() -> void
{
  visible = !visible;
  auto zone = visible ? client.height : 0;
  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, zone);
  wl_surface_commit(surface);
}

auto waybar::Bar::setup_widgets() -> void
{
  auto &left = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
  auto &center = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
  auto &right = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));

  auto &box1 = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
  window.add(box1);
  box1.set_homogeneous(true);
  box1.pack_start(left, true, true);
  box1.pack_start(center, false, false);
  box1.pack_end(right, true, true);

  auto &clock = *new waybar::modules::Clock();
  auto &workspace_selector = *new waybar::modules::WorkspaceSelector(*this);
  auto &battery = *new waybar::modules::Battery();
  auto &memory = *new waybar::modules::Memory();

  left.pack_start(workspace_selector, false, true, 0);
  // center.pack_start(workspace_selector, true, false, 10);
  right.pack_end(clock, false, false, 0);
  right.pack_end(battery, false, false, 0);
  right.pack_end(memory, false, false, 0);
}
