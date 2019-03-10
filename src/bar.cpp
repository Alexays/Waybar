#include "bar.hpp"
#include "client.hpp"
#include "factory.hpp"
#include "util/json.hpp"

waybar::Bar::Bar(const Client& client,
  std::unique_ptr<struct wl_output *> &&p_output, uint32_t p_wl_name)
  : client(client), window{Gtk::WindowType::WINDOW_TOPLEVEL},
    surface(nullptr), layer_surface(nullptr),
    output(std::move(p_output)), wl_name(p_wl_name),
    left_(Gtk::ORIENTATION_HORIZONTAL, 0), center_(Gtk::ORIENTATION_HORIZONTAL, 0),
    right_(Gtk::ORIENTATION_HORIZONTAL, 0), box_(Gtk::ORIENTATION_HORIZONTAL, 0)
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
  window.set_name("waybar");
  window.set_decorated(false);
  window.set_resizable(false);
  setupConfig();
  setupCss();

  auto wrap = reinterpret_cast<GtkWidget*>(window.gobj());
  gtk_widget_realize(wrap);
  GdkWindow *gdk_window = gtk_widget_get_window(wrap);
  gdk_wayland_window_set_use_custom_surface(gdk_window);
  surface = gdk_wayland_window_get_wl_surface(gdk_window);
}

void waybar::Bar::initBar()
{
  // Converting string to button code rn as to avoid doing it later
  auto setupAltFormatKeyForModule = [this](const std::string& module_name){
  	if (config_.isMember(module_name)) {
      Json::Value& module = config_[module_name];
      if (module.isMember("format-alt")) {
        if (module.isMember("format-alt-click")) {
          Json::Value& click = module["format-alt-click"];
          if (click.isString()) {
	        std::string str_click = click.asString();

	        if (str_click == "click-right") {
		      module["format-alt-click"] = 3u;
	        } else if (str_click == "click-middle") {
		      module["format-alt-click"] = 2u;
	        } else if (str_click == "click-backward") {
		      module["format-alt-click"] = 8u;
	        } else if (str_click == "click-forward") {
		      module["format-alt-click"] = 9u;
	        } else {
		      module["format-alt-click"] = 1u; // default click-left
	        }
          } else {
            module["format-alt-click"] = 1u;
          }
        } else {
	      module["format-alt-click"] = 1u;
        }

      }
    }
  };

  auto setupAltFormatKeyForModuleList = [this, &setupAltFormatKeyForModule](const char* module_list_name) {
  	if (config_.isMember(module_list_name)) {
  		Json::Value& modules = config_[module_list_name];
	    for (const Json::Value& module_name : modules) {
		    if (module_name.isString()) {
			    setupAltFormatKeyForModule(module_name.asString());
		    }
	    }
  	}
  };

  // Convert to button code for every module that is used.
  setupAltFormatKeyForModuleList("modules-left");
  setupAltFormatKeyForModuleList("modules-right");
  setupAltFormatKeyForModuleList("modules-center");
  std::size_t layer_top = config_["layer"] == "top"
    ? ZWLR_LAYER_SHELL_V1_LAYER_TOP : ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
  layer_surface = zwlr_layer_shell_v1_get_layer_surface(
    client.layer_shell, surface, *output, layer_top, "waybar");

  static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layerSurfaceHandleConfigure,
    .closed = layerSurfaceHandleClosed,
  };
  zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, this);

  std::size_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
    | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
  if (config_["position"] == "bottom") {
    anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
  } else {
    anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
  }

  auto height = config_["height"].isUInt() ? config_["height"].asUInt() : height_;
  auto width = config_["width"].isUInt() ? config_["width"].asUInt() : width_;
  zwlr_layer_surface_v1_set_anchor(layer_surface, anchor);
  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, height);
  zwlr_layer_surface_v1_set_size(layer_surface, width, height);

  wl_surface_commit(surface);

  setupWidgets();
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

bool waybar::Bar::isValidOutput(const Json::Value &config)
{
  bool found = true;
  if (config["output"].isArray()) {
    bool in_array = false;
    for (auto const &output : config["output"]) {
      if (output.isString() && output.asString() == output_name) {
        in_array = true;
        break;
      }
    }
    found = in_array;
  }
  if (config["output"].isString() && config["output"].asString() != output_name) {
    found = false;
  }
  return found;
}

void waybar::Bar::handleName(void* data, struct zxdg_output_v1* /*xdg_output*/,
  const char* name)
{
	auto o = static_cast<waybar::Bar *>(data);
  o->output_name = name;
  bool found = true;
  if (o->config_.isArray()) {
    bool in_array = false;
    for (auto const &config : o->config_) {
      if (config.isObject() && o->isValidOutput(config)) {
        in_array = true;
        o->config_ = config;
        break;
      }
    }
    found = in_array;
  } else {
    found = o->isValidOutput(o->config_);
  }
  if (!found) {
    wl_output_destroy(*o->output);
    zxdg_output_v1_destroy(o->xdg_output_);
  } else {
    o->initBar();
  }
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
  zwlr_layer_surface_v1_ack_configure(surface, serial);
  if (width != o->width_ || height != o->height_) {
    o->width_ = width;
    o->height_ = height;
    o->window.set_size_request(o->width_, o->height_);
    o->window.resize(o->width_, o->height_);

    int dummy_width, min_height;
    o->window.get_size(dummy_width, min_height);
    if (o->height_ < static_cast<uint32_t>(min_height)) {
      std::cout << fmt::format("Requested height: {} exceeds the minimum \
height: {} required by the modules", o->height_, min_height) << std::endl;
      o->height_ = min_height;
    }
    std::cout << fmt::format(
      "Bar configured (width: {}, height: {}) for output: {}",
      o->width_, o->height_, o->output_name) << std::endl;

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
  if (config_[pos].isArray()) {
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
        module->dp.connect([module, &name] {
          try {
            module->update();
          } catch (const std::exception& e) {
            std::cerr << name.asString() + ": " + e.what() << std::endl;
          }
        });
      } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
      }
    }
  }
}

auto waybar::Bar::setupWidgets() -> void
{
  window.add(box_);
  box_.pack_start(left_, true, true);
  box_.set_center_widget(center_);
  box_.pack_end(right_, true, true);

  Factory factory(*this, config_);
  getModules(factory, "modules-left");
  getModules(factory, "modules-center");
  getModules(factory, "modules-right");
  for (auto const& module : modules_left_) {
    left_.pack_start(*module, false, true, 0);
  }
  for (auto const& module : modules_center_) {
    center_.pack_start(*module, true, true, 0);
  }
  std::reverse(modules_right_.begin(), modules_right_.end());
  for (auto const& module : modules_right_) {
    right_.pack_end(*module, false, false, 0);
  }
  window.show_all();
}
