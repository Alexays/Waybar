#include "bar.hpp"
#include "client.hpp"
#include "factory.hpp"
#include "util/json.hpp"

waybar::Bar::Bar(const Client& client,
  std::unique_ptr<struct wl_output *> &&p_output, uint32_t p_wl_name)
  : client(client), window{Gtk::WindowType::WINDOW_TOPLEVEL},
    surface(nullptr), layer_surface(nullptr),
    output(std::move(p_output)), wl_name(p_wl_name)
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
  window.set_name("waybar");
  setupConfig();
  setupCss();
  setupWidgets();

  Gtk::Widget& wrap(window);
  gtk_widget_realize(wrap.gobj());
  GdkWindow *gdk_window = gtk_widget_get_window(wrap.gobj());
  gdk_wayland_window_set_use_custom_surface(gdk_window);
  surface = gdk_wayland_window_get_wl_surface(gdk_window);

  std::size_t layer_top = config_["layer"] == "top"
    ? ZWLR_LAYER_SHELL_V1_LAYER_TOP : ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
  layer_surface = zwlr_layer_shell_v1_get_layer_surface(
    client.layer_shell, surface, *output, layer_top, "waybar");

  static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layerSurfaceHandleConfigure,
    .closed = layerSurfaceHandleClosed,
  };
  zwlr_layer_surface_v1_add_listener(layer_surface,
    &layer_surface_listener, this);

  std::size_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
    | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
  if (config_["position"] == "bottom") {
    anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
  } else {
    anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
  }

  auto height = config_["height"] ? config_["height"].asUInt() : height_;
  auto width = config_["width"] ? config_["width"].asUInt() : width_;
  zwlr_layer_surface_v1_set_anchor(layer_surface, anchor);
  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, height);
  zwlr_layer_surface_v1_set_size(layer_surface, width, height);

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
  o->output_name = name;
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
  zwlr_layer_surface_v1_ack_configure(surface, serial);
  if (width != o->width_ || height != o->height_) {
    o->width_ = width;
    o->height_ = height;
    std::cout << fmt::format(
      "Bar configured (width: {}, height: {}) for output: {}",
      o->width_, o->height_, o->output_name) << std::endl;
    o->window.set_size_request(o->width_, o->height_);
    o->window.resize(o->width_, o->height_);
    zwlr_layer_surface_v1_set_exclusive_zone(surface, o->height_);
    zwlr_layer_surface_v1_set_size(surface, o->width_, o->height_);
    wl_surface_commit(o->surface);
  }
}

void waybar::Bar::layerSurfaceHandleClosed(void* data,
  struct zwlr_layer_surface_v1* /*surface*/)
{
  auto o = static_cast<waybar::Bar *>(data);
  std::cout << "Bar removed from output: " + o->output_name << std::endl;
  zwlr_layer_surface_v1_destroy(o->layer_surface);
  wl_output_destroy(*o->output);
  zxdg_output_v1_destroy(o->xdg_output_);
  o->modules_left_.clear();
  o->modules_center_.clear();
  o->modules_right_.clear();
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
  std::ifstream file(client.config_file);
  if (!file.is_open()) {
    throw std::runtime_error("Can't open config file");
  }
  std::string str((std::istreambuf_iterator<char>(file)),
    std::istreambuf_iterator<char>());
  util::JsonParser parser;
  config_ = parser.parse(str);
}

auto waybar::Bar::setupCss() -> void
{
  css_provider_ = Gtk::CssProvider::create();
  style_context_ = Gtk::StyleContext::create();

  // Load our css file, wherever that may be hiding
  if (css_provider_->load_from_path(client.css_file)) {
    Glib::RefPtr<Gdk::Screen> screen = window.get_screen();
    style_context_->add_provider_for_screen(screen, css_provider_,
      GTK_STYLE_PROVIDER_PRIORITY_USER);
  }
}

void waybar::Bar::getModules(const Factory& factory, const std::string& pos)
{
  if (config_[pos]) {
    for (const auto &name : config_[pos]) {
      try {
        auto module = factory.makeModule(name.asString());
        if (pos == "modules-left") {
          modules_left_.emplace_back(module);
        }
        if (pos == "modules-center") {
          modules_center_.emplace_back(module);
        }
        if (pos == "modules-right") {
          modules_right_.emplace_back(module);
        }
        module->dp.connect([module] { module->update(); });
      } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
      }
    }
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
  getModules(factory, "modules-left");
  getModules(factory, "modules-center");
  getModules(factory, "modules-right");
  for (auto const& module : modules_left_) {
    left.pack_start(*module, false, true, 0);
  }
  for (auto const& module : modules_center_) {
    center.pack_start(*module, true, true, 0);
  }
  std::reverse(modules_right_.begin(), modules_right_.end());
  for (auto const& module : modules_right_) {
    right.pack_end(*module, false, false, 0);
  }
}
