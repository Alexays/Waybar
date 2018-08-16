#include "bar.hpp"
#include "client.hpp"
#include "factory.hpp"
#include "util/json.hpp"

waybar::Bar::Bar(Client &client, std::unique_ptr<struct wl_output *> &&p_output)
  : client(client), window{Gtk::WindowType::WINDOW_TOPLEVEL},
    output(std::move(p_output))
{
  static const struct zxdg_output_v1_listener xdgOutputListener = {
    .logical_position = handleLogicalPosition,
    .logical_size = handleLogicalSize,
    .done = handleDone,
    .name = handleName,
    .description = handleDescription,
  };
  xdg_output_ =
    zxdg_output_manager_v1_get_xdg_output(client.xdg_output_manager, *output);
	zxdg_output_v1_add_listener(xdg_output_, &xdgOutputListener, this);
  window.set_title("waybar");
  window.set_decorated(false);
  setupConfig();
  setupCss();
  setupWidgets();
  if (config_["height"]) {
    height_ = config_["height"].asUInt();
  }
  Gtk::Widget& wrap(window);
  gtk_widget_realize(wrap.gobj());
  GdkWindow *gdk_window = gtk_widget_get_window(wrap.gobj());
  gdk_wayland_window_set_use_custom_surface(gdk_window);
  surface = gdk_wayland_window_get_wl_surface(gdk_window);
  std::size_t layer_top = config_["layer"] == "top"
    ? ZWLR_LAYER_SHELL_V1_LAYER_TOP : ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
  layer_surface = zwlr_layer_shell_v1_get_layer_surface(
    client.layer_shell, surface, *output, layer_top, "waybar");
  std::size_t position_bottom = config_["position"] == "bottom"
    ? ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM : ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
  zwlr_layer_surface_v1_set_anchor(layer_surface, position_bottom
    | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
  zwlr_layer_surface_v1_set_size(layer_surface, width_, height_);
  static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layerSurfaceHandleConfigure,
    .closed = layerSurfaceHandleClosed,
  };
  zwlr_layer_surface_v1_add_listener(layer_surface,
    &layer_surface_listener, this);
  wl_surface_commit(surface);
}

void waybar::Bar::handleLogicalPosition(void* /*data*/,
  struct zxdg_output_v1* /*zxdg_output_v1*/, int32_t /*x*/, int32_t /*y*/)
{
  // Nothing here
}

void waybar::Bar::handleLogicalSize(void* /*data*/,
  struct zxdg_output_v1* /*zxdg_output_v1*/, int32_t /*width*/,
  int32_t /*height*/)
{
  // Nothing here
}

void waybar::Bar::handleDone(void* /*data*/,
  struct zxdg_output_v1* /*zxdg_output_v1*/)
{
  // Nothing here
}

void waybar::Bar::handleName(void* data, struct zxdg_output_v1* /*xdg_output*/,
  const char* name)
{
	auto o = static_cast<waybar::Bar *>(data);
  o->outputName = name;
}

void waybar::Bar::handleDescription(void* /*data*/,
  struct zxdg_output_v1* /*zxdg_output_v1*/, const char* /*description*/)
{
  // Nothing here
}

void waybar::Bar::layerSurfaceHandleConfigure(void* data,
  struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t width,
  uint32_t height)
{
  auto o = static_cast<waybar::Bar *>(data);
  o->window.show_all();
  o->setWidth(o->config_["width"] ? o->config_["width"].asUInt() : width);
  zwlr_layer_surface_v1_ack_configure(surface, serial);
  if (o->height_ != height) {
    height = o->height_;
    std::cout << fmt::format("New Height: {}", height) << std::endl;
    zwlr_layer_surface_v1_set_size(surface, o->width_, height);
    zwlr_layer_surface_v1_set_exclusive_zone(surface, o->visible ? height : 0);
    wl_surface_commit(o->surface);
  }
}

void waybar::Bar::layerSurfaceHandleClosed(void* data,
  struct zwlr_layer_surface_v1* /*surface*/)
{
  auto o = static_cast<waybar::Bar *>(data);
  zwlr_layer_surface_v1_destroy(o->layer_surface);
  o->layer_surface = nullptr;
  wl_surface_destroy(o->surface);
  o->surface = nullptr;
  o->window.close();
}

auto waybar::Bar::setWidth(uint32_t width) -> void
{
  if (width == width_) {
    return;
  }
  std::cout << fmt::format("Bar width configured: {}", width) << std::endl;
  width_ = width;
  window.set_size_request(width);
  window.resize(width, height_);
  zwlr_layer_surface_v1_set_size(layer_surface, width, height_ + 1);
  wl_surface_commit(surface);
}

auto waybar::Bar::toggle() -> void
{
  visible = !visible;
  auto zone = visible ? height_ : 0;
  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, zone);
  wl_surface_commit(surface);
}

auto waybar::Bar::setupConfig() -> void
{
  util::JsonParser parser;
  std::ifstream file(client.config_file);
  if (!file.is_open()) {
    throw std::runtime_error("Can't open config file");
  }
  std::string str((std::istreambuf_iterator<char>(file)),
    std::istreambuf_iterator<char>());
  config_ = parser.parse(str);
}

auto waybar::Bar::setupCss() -> void
{
  css_provider_ = Gtk::CssProvider::create();
  style_context_ = Gtk::StyleContext::create();

  // load our css file, wherever that may be hiding
  if (css_provider_->load_from_path(client.css_file)) {
    Glib::RefPtr<Gdk::Screen> screen = window.get_screen();
    style_context_->add_provider_for_screen(screen, css_provider_,
      GTK_STYLE_PROVIDER_PRIORITY_USER);
  }
}

auto waybar::Bar::setupWidgets() -> void
{
  auto &left = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
  auto &center = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
  auto &right = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));

  auto &box = *Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
  window.add(box);
  box.pack_start(left, true, true);
  box.set_center_widget(center);
  box.pack_end(right, true, true);

  Factory factory(*this, config_);

  if (config_["modules-left"]) {
    for (const auto &name : config_["modules-left"]) {
      auto module = factory.makeModule(name.asString());
      if (module != nullptr) {
        left.pack_start(*module, false, true, 0);
      }
    }
  }
  if (config_["modules-center"]) {
    for (const auto &name : config_["modules-center"]) {
      auto module = factory.makeModule(name.asString());
      if (module != nullptr) {
        center.pack_start(*module, true, false, 0);
      }
    }
  }
  if (config_["modules-right"]) {
    std::reverse(config_["modules-right"].begin(), config_["modules-right"].end());
    for (const auto &name : config_["modules-right"]) {
      auto module = factory.makeModule(name.asString());
      if (module != nullptr) {
        right.pack_end(*module, false, false, 0);
      }
    }
  }
}
