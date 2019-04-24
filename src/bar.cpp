#include "bar.hpp"
#include "client.hpp"
#include "factory.hpp"

waybar::Bar::Bar(struct waybar_output* w_output)
    : output(w_output),
      window{Gtk::WindowType::WINDOW_TOPLEVEL},
      surface(nullptr),
      layer_surface(nullptr),
      left_(Gtk::ORIENTATION_HORIZONTAL, 0),
      center_(Gtk::ORIENTATION_HORIZONTAL, 0),
      right_(Gtk::ORIENTATION_HORIZONTAL, 0),
      box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  window.set_title("waybar");
  window.set_name("waybar");
  window.set_decorated(false);

  if (output->config["position"] == "right" || output->config["position"] == "left") {
    height_ = 0;
    width_ = 1;
  }
  window.set_size_request(width_, height_);

  auto gtk_window = window.gobj();
  auto gtk_widget = GTK_WIDGET(gtk_window);
  gtk_widget_realize(gtk_widget);
  auto gdk_window = window.get_window()->gobj();
  gdk_wayland_window_set_use_custom_surface(gdk_window);
  surface = gdk_wayland_window_get_wl_surface(gdk_window);

  std::size_t layer = output->config["layer"] == "top" ? ZWLR_LAYER_SHELL_V1_LAYER_TOP
                                                       : ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
  auto client = waybar::Client::inst();
  layer_surface = zwlr_layer_shell_v1_get_layer_surface(
      client->layer_shell, surface, output->output, layer, "waybar");
  static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
      .configure = layerSurfaceHandleConfigure,
      .closed = layerSurfaceHandleClosed,
  };
  zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, this);

  auto height = output->config["height"].isUInt() ? output->config["height"].asUInt() : height_;
  auto width = output->config["width"].isUInt() ? output->config["width"].asUInt() : width_;

  window.signal_configure_event().connect_notify([&](GdkEventConfigure* ev) {
    auto tmp_height = height_;
    auto tmp_width = width_;
    if (ev->height > static_cast<int>(height_)) {
      // Default minimal value
      if (height_ != 1) {
        std::cout << fmt::format(MIN_HEIGHT_MSG, height_, ev->height) << std::endl;
      }
      if (output->config["height"].isUInt()) {
        std::cout << "Height size is defined in the config file so it will stay like that"
                  << std::endl;
      } else {
        tmp_height = ev->height;
      }
    }
    if (ev->width > static_cast<int>(width_)) {
      // Default minimal value
      if (width_ != 1) {
        std::cout << fmt::format(MIN_WIDTH_MSG, width_, ev->width) << std::endl;
      }
      if (output->config["width"].isUInt()) {
        std::cout << "Height size is defined in the config file so it will stay like that"
                  << std::endl;
      } else {
        tmp_width = ev->width;
      }
    }
    if (tmp_width != width_ || tmp_height != height_) {
      zwlr_layer_surface_v1_set_size(layer_surface, tmp_width, tmp_height);
    }
  });

  std::size_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
  if (output->config["position"] == "bottom") {
    anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
  } else if (output->config["position"] == "left") {
    anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
  } else if (output->config["position"] == "right") {
    anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
  }
  if (anchor == ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM || anchor == ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
    anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
  } else if (anchor == ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT ||
             anchor == ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
    anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    left_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    center_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    right_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    box_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    vertical = true;
  }

  zwlr_layer_surface_v1_set_anchor(layer_surface, anchor);
  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, vertical ? width : height);
  zwlr_layer_surface_v1_set_size(layer_surface, width, height);

  wl_surface_commit(surface);
  wl_display_roundtrip(client->wl_display);

  setupWidgets();
}

// Converting string to button code rn as to avoid doing it later
void waybar::Bar::setupAltFormatKeyForModule(const std::string& module_name) {
  if (output->config.isMember(module_name)) {
    Json::Value& module = output->config[module_name];
    if (module.isMember("format-alt")) {
      if (module.isMember("format-alt-click")) {
        Json::Value& click = module["format-alt-click"];
        if (click.isString()) {
          std::string str_click = click.asString();

          if (str_click == "click-right") {
            module["format-alt-click"] = 3U;
          } else if (str_click == "click-middle") {
            module["format-alt-click"] = 2U;
          } else if (str_click == "click-backward") {
            module["format-alt-click"] = 8U;
          } else if (str_click == "click-forward") {
            module["format-alt-click"] = 9U;
          } else {
            module["format-alt-click"] = 1U;  // default click-left
          }
        } else {
          module["format-alt-click"] = 1U;
        }
      } else {
        module["format-alt-click"] = 1U;
      }
    }
  }
}

void waybar::Bar::setupAltFormatKeyForModuleList(const char* module_list_name) {
  if (output->config.isMember(module_list_name)) {
    Json::Value& modules = output->config[module_list_name];
    for (const Json::Value& module_name : modules) {
      if (module_name.isString()) {
        setupAltFormatKeyForModule(module_name.asString());
      }
    }
  }
}

void waybar::Bar::handleSignal(int signal) {
  for (auto& module : modules_left_) {
    auto* custom = dynamic_cast<waybar::modules::Custom*>(module.get());
    if (custom != nullptr) {
      custom->refresh(signal);
    }
  }
  for (auto& module : modules_center_) {
    auto* custom = dynamic_cast<waybar::modules::Custom*>(module.get());
    if (custom != nullptr) {
      custom->refresh(signal);
    }
  }
  for (auto& module : modules_right_) {
    auto* custom = dynamic_cast<waybar::modules::Custom*>(module.get());
    if (custom != nullptr) {
      custom->refresh(signal);
    }
  }
}

void waybar::Bar::layerSurfaceHandleConfigure(void* data, struct zwlr_layer_surface_v1* surface,
                                              uint32_t serial, uint32_t width, uint32_t height) {
  auto o = static_cast<waybar::Bar*>(data);
  if (width != o->width_ || height != o->height_) {
    o->width_ = width;
    o->height_ = height;
    o->window.set_size_request(o->width_, o->height_);
    o->window.resize(o->width_, o->height_);
    zwlr_layer_surface_v1_set_exclusive_zone(o->layer_surface,
                                             o->vertical ? o->width_ : o->height_);
    std::cout << fmt::format(BAR_SIZE_MSG,
                             o->width_ == 1 ? "auto" : std::to_string(o->width_),
                             o->height_ == 1 ? "auto" : std::to_string(o->height_),
                             o->output->name)
              << std::endl;
    wl_surface_commit(o->surface);
  }
  zwlr_layer_surface_v1_ack_configure(surface, serial);
}

void waybar::Bar::layerSurfaceHandleClosed(void* data, struct zwlr_layer_surface_v1* /*surface*/) {
  auto o = static_cast<waybar::Bar*>(data);
  zwlr_layer_surface_v1_destroy(o->layer_surface);
  o->modules_left_.clear();
  o->modules_center_.clear();
  o->modules_right_.clear();
}

auto waybar::Bar::toggle() -> void {
  visible = !visible;
  auto zone = visible ? height_ : 0;
  if (!visible) {
    window.get_style_context()->add_class("hidded");
  } else {
    window.get_style_context()->remove_class("hidded");
  }
  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, zone);
  wl_surface_commit(surface);
}

void waybar::Bar::getModules(const Factory& factory, const std::string& pos) {
  if (output->config[pos].isArray()) {
    for (const auto& name : output->config[pos]) {
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

auto waybar::Bar::setupWidgets() -> void {
  window.add(box_);
  box_.pack_start(left_, true, true);
  box_.set_center_widget(center_);
  box_.pack_end(right_, true, true);

  // Convert to button code for every module that is used.
  setupAltFormatKeyForModuleList("modules-left");
  setupAltFormatKeyForModuleList("modules-right");
  setupAltFormatKeyForModuleList("modules-center");

  Factory factory(*this, output->config);
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
