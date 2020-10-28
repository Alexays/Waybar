#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
#endif

#include <spdlog/spdlog.h>

#include "bar.hpp"
#include "client.hpp"
#include "factory.hpp"

namespace waybar {
static constexpr const char* MIN_HEIGHT_MSG =
    "Requested height: {} exceeds the minimum height: {} required by the modules";

static constexpr const char* MIN_WIDTH_MSG =
    "Requested width: {} exceeds the minimum width: {} required by the modules";

static constexpr const char* BAR_SIZE_MSG = "Bar configured (width: {}, height: {}) for output: {}";

static constexpr const char* SIZE_DEFINED =
    "{} size is defined in the config file so it will stay like that";

#ifdef HAVE_GTK_LAYER_SHELL
struct GLSSurfaceImpl : public BarSurface, public sigc::trackable {
  GLSSurfaceImpl(Gtk::Window& window, struct waybar_output& output) : window_{window} {
    output_name_ = output.name;
    // this has to be executed before GtkWindow.realize
    gtk_layer_init_for_window(window_.gobj());
    gtk_layer_set_keyboard_interactivity(window.gobj(), FALSE);
    gtk_layer_set_monitor(window_.gobj(), output.monitor->gobj());
    gtk_layer_set_namespace(window_.gobj(), "waybar");

    window.signal_configure_event().connect_notify(
        sigc::mem_fun(*this, &GLSSurfaceImpl::onConfigure));
  }

  void setExclusiveZone(bool enable) override {
    if (enable) {
      gtk_layer_auto_exclusive_zone_enable(window_.gobj());
    } else {
      gtk_layer_set_exclusive_zone(window_.gobj(), 0);
    }
  }

  void setMargins(const struct bar_margins& margins) override {
    gtk_layer_set_margin(window_.gobj(), GTK_LAYER_SHELL_EDGE_LEFT, margins.left);
    gtk_layer_set_margin(window_.gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, margins.right);
    gtk_layer_set_margin(window_.gobj(), GTK_LAYER_SHELL_EDGE_TOP, margins.top);
    gtk_layer_set_margin(window_.gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, margins.bottom);
  }

  void setLayer(const std::string_view& value) override {
    auto layer = GTK_LAYER_SHELL_LAYER_BOTTOM;
    if (value == "top") {
      layer = GTK_LAYER_SHELL_LAYER_TOP;
    } else if (value == "overlay") {
      layer = GTK_LAYER_SHELL_LAYER_OVERLAY;
    }
    gtk_layer_set_layer(window_.gobj(), layer);
  }

  void setPosition(const std::string_view& position) override {
    auto unanchored = GTK_LAYER_SHELL_EDGE_BOTTOM;
    vertical_ = false;
    if (position == "bottom") {
      unanchored = GTK_LAYER_SHELL_EDGE_TOP;
    } else if (position == "left") {
      unanchored = GTK_LAYER_SHELL_EDGE_RIGHT;
      vertical_ = true;
    } else if (position == "right") {
      vertical_ = true;
      unanchored = GTK_LAYER_SHELL_EDGE_LEFT;
    }
    for (auto edge : {GTK_LAYER_SHELL_EDGE_LEFT,
                      GTK_LAYER_SHELL_EDGE_RIGHT,
                      GTK_LAYER_SHELL_EDGE_TOP,
                      GTK_LAYER_SHELL_EDGE_BOTTOM}) {
      gtk_layer_set_anchor(window_.gobj(), edge, unanchored != edge);
    }
  }

  void setSize(uint32_t width, uint32_t height) override {
    width_ = width;
    height_ = height;
    window_.set_size_request(width_, height_);
  };

 private:
  Gtk::Window& window_;
  std::string  output_name_;
  uint32_t     width_;
  uint32_t     height_;
  bool         vertical_ = false;

  void onConfigure(GdkEventConfigure* ev) {
    /*
     * GTK wants new size for the window.
     * Actual resizing and management of the exclusve zone is handled within the gtk-layer-shell
     * code. This event handler only updates stored size of the window and prints some warnings.
     *
     * Note: forced resizing to a window smaller than required by GTK would not work with
     * gtk-layer-shell.
     */
    if (vertical_) {
      if (width_ > 1 && ev->width > static_cast<int>(width_)) {
        spdlog::warn(MIN_WIDTH_MSG, width_, ev->width);
      }
    } else {
      if (height_ > 1 && ev->height > static_cast<int>(height_)) {
        spdlog::warn(MIN_HEIGHT_MSG, height_, ev->height);
      }
    }
    width_ = ev->width;
    height_ = ev->height;
    spdlog::info(BAR_SIZE_MSG, width_, height_, output_name_);
  }
};
#endif

struct RawSurfaceImpl : public BarSurface, public sigc::trackable {
  RawSurfaceImpl(Gtk::Window& window, struct waybar_output& output) : window_{window} {
    output_ = gdk_wayland_monitor_get_wl_output(output.monitor->gobj());
    output_name_ = output.name;

    window.signal_realize().connect_notify(sigc::mem_fun(*this, &RawSurfaceImpl::onRealize));
    window.signal_map_event().connect_notify(sigc::mem_fun(*this, &RawSurfaceImpl::onMap));
    window.signal_configure_event().connect_notify(
        sigc::mem_fun(*this, &RawSurfaceImpl::onConfigure));

    if (window.get_realized()) {
      onRealize();
    }
  }

  void setExclusiveZone(bool enable) override {
    exclusive_zone_ = enable;
    if (layer_surface_) {
      auto zone = 0;
      if (enable) {
        // exclusive zone already includes margin for anchored edge,
        // only opposite margin should be added
        if ((anchor_ & VERTICAL_ANCHOR) == VERTICAL_ANCHOR) {
          zone += width_;
          zone += (anchor_ & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) ? margins_.right : margins_.left;
        } else {
          zone += height_;
          zone += (anchor_ & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) ? margins_.bottom : margins_.top;
        }
      }
      spdlog::debug("Set exclusive zone {} for output {}", zone, output_name_);
      zwlr_layer_surface_v1_set_exclusive_zone(layer_surface_, zone);
    }
  }

  void setLayer(const std::string_view& layer) override {
    layer_ = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
    if (layer == "top") {
      layer_ = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
    } else if (layer == "overlay") {
      layer_ = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
    }
    // updating already mapped window
    if (layer_surface_) {
      if (zwlr_layer_surface_v1_get_version(layer_surface_) >=
          ZWLR_LAYER_SURFACE_V1_SET_LAYER_SINCE_VERSION) {
        zwlr_layer_surface_v1_set_layer(layer_surface_, layer_);
        commit();
      } else {
        spdlog::warn("Unable to set layer: layer-shell interface version is too old");
      }
    }
  }

  void setMargins(const struct bar_margins& margins) override {
    margins_ = margins;
    // updating already mapped window
    if (layer_surface_) {
      zwlr_layer_surface_v1_set_margin(
          layer_surface_, margins_.top, margins_.right, margins_.bottom, margins_.left);
      commit();
    }
  }

  void setPosition(const std::string_view& position) override {
    anchor_ = HORIZONTAL_ANCHOR | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    if (position == "bottom") {
      anchor_ = HORIZONTAL_ANCHOR | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    } else if (position == "left") {
      anchor_ = VERTICAL_ANCHOR | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
    } else if (position == "right") {
      anchor_ = VERTICAL_ANCHOR | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    }

    // updating already mapped window
    if (layer_surface_) {
      zwlr_layer_surface_v1_set_anchor(layer_surface_, anchor_);
      commit();
    }
  }

  void setSize(uint32_t width, uint32_t height) override {
    width_ = width;
    height_ = height;
    // layer_shell.configure handler should update exclusive zone if size changes
    window_.set_size_request(width, height);
  };

 private:
  constexpr static uint8_t VERTICAL_ANCHOR =
      ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
  constexpr static uint8_t HORIZONTAL_ANCHOR =
      ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;

  Gtk::Window&       window_;
  std::string        output_name_;
  uint32_t           width_;
  uint32_t           height_;
  uint8_t            anchor_ = HORIZONTAL_ANCHOR | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
  bool               exclusive_zone_ = true;
  struct bar_margins margins_;

  zwlr_layer_shell_v1_layer     layer_ = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
  struct wl_output*             output_ = nullptr;
  struct wl_surface*            surface_ = nullptr;
  struct zwlr_layer_surface_v1* layer_surface_ = nullptr;

  void onRealize() {
    auto gdk_window = window_.get_window()->gobj();
    gdk_wayland_window_set_use_custom_surface(gdk_window);
  }

  void onMap(GdkEventAny* ev) {
    auto client = Client::inst();
    auto gdk_window = window_.get_window()->gobj();
    surface_ = gdk_wayland_window_get_wl_surface(gdk_window);

    layer_surface_ = zwlr_layer_shell_v1_get_layer_surface(
        client->layer_shell, surface_, output_, layer_, "waybar");

    zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface_, false);

    zwlr_layer_surface_v1_set_anchor(layer_surface_, anchor_);
    zwlr_layer_surface_v1_set_margin(
        layer_surface_, margins_.top, margins_.right, margins_.bottom, margins_.left);
    setSurfaceSize(width_, height_);
    setExclusiveZone(exclusive_zone_);

    static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
        .configure = onSurfaceConfigure,
        .closed = onSurfaceClosed,
    };
    zwlr_layer_surface_v1_add_listener(layer_surface_, &layer_surface_listener, this);

    wl_surface_commit(surface_);
    wl_display_roundtrip(client->wl_display);
  }

  void onConfigure(GdkEventConfigure* ev) {
    /*
     * GTK wants new size for the window.
     *
     * Prefer configured size if it's non-default.
     * If the size is not set and the window is smaller than requested by GTK, request resize from
     * layer surface.
     */
    auto tmp_height = height_;
    auto tmp_width = width_;
    if (ev->height > static_cast<int>(height_)) {
      // Default minimal value
      if (height_ > 1) {
        spdlog::warn(MIN_HEIGHT_MSG, height_, ev->height);
      }
      /*
      if (config["height"].isUInt()) {
        spdlog::info(SIZE_DEFINED, "Height");
      } else */
      tmp_height = ev->height;
    }
    if (ev->width > static_cast<int>(width_)) {
      // Default minimal value
      if (width_ > 1) {
        spdlog::warn(MIN_WIDTH_MSG, width_, ev->width);
      }
      /*
      if (config["width"].isUInt()) {
        spdlog::info(SIZE_DEFINED, "Width");
      } else */
      tmp_width = ev->width;
    }
    if (tmp_width != width_ || tmp_height != height_) {
      setSurfaceSize(tmp_width, tmp_height);
    }
  }

  void commit() {
    if (window_.get_mapped()) {
      wl_surface_commit(surface_);
    }
  }

  void setSurfaceSize(uint32_t width, uint32_t height) {
    /* If the client is anchored to two opposite edges, layer_surface.configure will return
     * size without margins for the axis.
     * layer_surface.set_size, however, expects size with margins for the anchored axis.
     * This is not specified by wlr-layer-shell and based on actual behavior of sway.
     */
    bool vertical = (anchor_ & VERTICAL_ANCHOR) == VERTICAL_ANCHOR;
    if (vertical && height > 1) {
      height += margins_.top + margins_.bottom;
    }
    if (!vertical && width > 1) {
      width += margins_.right + margins_.left;
    }
    spdlog::debug("Set surface size {}x{} for output {}", width, height, output_name_);
    zwlr_layer_surface_v1_set_size(layer_surface_, width, height);
  }

  static void onSurfaceConfigure(void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial,
                                 uint32_t width, uint32_t height) {
    auto o = static_cast<RawSurfaceImpl*>(data);
    if (width != o->width_ || height != o->height_) {
      o->width_ = width;
      o->height_ = height;
      o->window_.set_size_request(o->width_, o->height_);
      o->window_.resize(o->width_, o->height_);
      o->setExclusiveZone(o->exclusive_zone_);
      spdlog::info(BAR_SIZE_MSG,
                   o->width_ == 1 ? "auto" : std::to_string(o->width_),
                   o->height_ == 1 ? "auto" : std::to_string(o->height_),
                   o->output_name_);
      wl_surface_commit(o->surface_);
    }
    zwlr_layer_surface_v1_ack_configure(surface, serial);
  }

  static void onSurfaceClosed(void* data, struct zwlr_layer_surface_v1* /* surface */) {
    auto o = static_cast<RawSurfaceImpl*>(data);
    if (o->layer_surface_) {
      zwlr_layer_surface_v1_destroy(o->layer_surface_);
      o->layer_surface_ = nullptr;
    }
  }
};

};  // namespace waybar

waybar::Bar::Bar(struct waybar_output* w_output, const Json::Value& w_config)
    : output(w_output),
      config(w_config),
      window{Gtk::WindowType::WINDOW_TOPLEVEL},
      left_(Gtk::ORIENTATION_HORIZONTAL, 0),
      center_(Gtk::ORIENTATION_HORIZONTAL, 0),
      right_(Gtk::ORIENTATION_HORIZONTAL, 0),
      box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  window.set_title("waybar");
  window.set_name("waybar");
  window.set_decorated(false);
  window.get_style_context()->add_class(output->name);
  window.get_style_context()->add_class(config["name"].asString());
  window.get_style_context()->add_class(config["position"].asString());
  left_.get_style_context()->add_class("modules-left");
  center_.get_style_context()->add_class("modules-center");
  right_.get_style_context()->add_class("modules-right");

  auto position = config["position"].asString();

  if (position == "right" || position == "left") {
    left_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    center_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    right_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    box_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    vertical = true;

    height_ = 0;
    width_ = 1;
  }
  height_ = config["height"].isUInt() ? config["height"].asUInt() : height_;
  width_ = config["width"].isUInt() ? config["width"].asUInt() : width_;

  struct bar_margins margins_;

  if (config["margin-top"].isInt() || config["margin-right"].isInt() ||
      config["margin-bottom"].isInt() || config["margin-left"].isInt()) {
    margins_ = {
        config["margin-top"].isInt() ? config["margin-top"].asInt() : 0,
        config["margin-right"].isInt() ? config["margin-right"].asInt() : 0,
        config["margin-bottom"].isInt() ? config["margin-bottom"].asInt() : 0,
        config["margin-left"].isInt() ? config["margin-left"].asInt() : 0,
    };
  } else if (config["margin"].isString()) {
    std::istringstream       iss(config["margin"].asString());
    std::vector<std::string> margins{std::istream_iterator<std::string>(iss), {}};
    try {
      if (margins.size() == 1) {
        auto gaps = std::stoi(margins[0], nullptr, 10);
        margins_ = {.top = gaps, .right = gaps, .bottom = gaps, .left = gaps};
      }
      if (margins.size() == 2) {
        auto vertical_margins = std::stoi(margins[0], nullptr, 10);
        auto horizontal_margins = std::stoi(margins[1], nullptr, 10);
        margins_ = {.top = vertical_margins,
                    .right = horizontal_margins,
                    .bottom = vertical_margins,
                    .left = horizontal_margins};
      }
      if (margins.size() == 3) {
        auto horizontal_margins = std::stoi(margins[1], nullptr, 10);
        margins_ = {.top = std::stoi(margins[0], nullptr, 10),
                    .right = horizontal_margins,
                    .bottom = std::stoi(margins[2], nullptr, 10),
                    .left = horizontal_margins};
      }
      if (margins.size() == 4) {
        margins_ = {.top = std::stoi(margins[0], nullptr, 10),
                    .right = std::stoi(margins[1], nullptr, 10),
                    .bottom = std::stoi(margins[2], nullptr, 10),
                    .left = std::stoi(margins[3], nullptr, 10)};
      }
    } catch (...) {
      spdlog::warn("Invalid margins: {}", config["margin"].asString());
    }
  } else if (config["margin"].isInt()) {
    auto gaps = config["margin"].asInt();
    margins_ = {.top = gaps, .right = gaps, .bottom = gaps, .left = gaps};
  }

#ifdef HAVE_GTK_LAYER_SHELL
  bool use_gls = config["gtk-layer-shell"].isBool() ? config["gtk-layer-shell"].asBool() : true;
  if (use_gls) {
    surface_impl_ = std::make_unique<GLSSurfaceImpl>(window, *output);
  } else
#endif
  {
    surface_impl_ = std::make_unique<RawSurfaceImpl>(window, *output);
  }

  if (config["layer"].isString()) {
    surface_impl_->setLayer(config["layer"].asString());
  }
  surface_impl_->setExclusiveZone(true);
  surface_impl_->setMargins(margins_);
  surface_impl_->setPosition(position);
  surface_impl_->setSize(width_, height_);

  window.signal_map_event().connect_notify(sigc::mem_fun(*this, &Bar::onMap));

  setupWidgets();
  window.show_all();
}

void waybar::Bar::onMap(GdkEventAny*) {
  /*
   * Obtain a pointer to the custom layer surface for modules that require it (idle_inhibitor).
   */
  auto gdk_window = window.get_window()->gobj();
  surface = gdk_wayland_window_get_wl_surface(gdk_window);
}

void waybar::Bar::setVisible(bool value) {
  visible = value;
  if (!visible) {
    window.get_style_context()->add_class("hidden");
    window.set_opacity(0);
  } else {
    window.get_style_context()->remove_class("hidden");
    window.set_opacity(1);
  }
  surface_impl_->setExclusiveZone(visible);
}

void waybar::Bar::toggle() { setVisible(!visible); }

// Converting string to button code rn as to avoid doing it later
void waybar::Bar::setupAltFormatKeyForModule(const std::string& module_name) {
  if (config.isMember(module_name)) {
    Json::Value& module = config[module_name];
    if (module.isMember("format-alt")) {
      if (module.isMember("format-alt-click")) {
        Json::Value& click = module["format-alt-click"];
        if (click.isString()) {
          if (click == "click-right") {
            module["format-alt-click"] = 3U;
          } else if (click == "click-middle") {
            module["format-alt-click"] = 2U;
          } else if (click == "click-backward") {
            module["format-alt-click"] = 8U;
          } else if (click == "click-forward") {
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
  if (config.isMember(module_list_name)) {
    Json::Value& modules = config[module_list_name];
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

void waybar::Bar::getModules(const Factory& factory, const std::string& pos) {
  if (config[pos].isArray()) {
    for (const auto& name : config[pos]) {
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
            spdlog::error("{}: {}", name.asString(), e.what());
          }
        });
      } catch (const std::exception& e) {
        spdlog::warn("module {}: {}", name.asString(), e.what());
      }
    }
  }
}

auto waybar::Bar::setupWidgets() -> void {
  window.add(box_);
  box_.pack_start(left_, false, false);
  box_.set_center_widget(center_);
  box_.pack_end(right_, false, false);

  // Convert to button code for every module that is used.
  setupAltFormatKeyForModuleList("modules-left");
  setupAltFormatKeyForModuleList("modules-right");
  setupAltFormatKeyForModuleList("modules-center");

  Factory factory(*this, config);
  getModules(factory, "modules-left");
  getModules(factory, "modules-center");
  getModules(factory, "modules-right");
  for (auto const& module : modules_left_) {
    left_.pack_start(*module, false, false);
  }
  for (auto const& module : modules_center_) {
    center_.pack_start(*module, false, false);
  }
  std::reverse(modules_right_.begin(), modules_right_.end());
  for (auto const& module : modules_right_) {
    right_.pack_end(*module, false, false);
  }
}
