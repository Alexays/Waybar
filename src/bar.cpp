#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
#endif

#include <spdlog/spdlog.h>

#include <type_traits>

#include "bar.hpp"
#include "client.hpp"
#include "factory.hpp"
#include "group.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#ifdef HAVE_SWAY
#include "modules/sway/bar.hpp"
#endif

namespace waybar {
static constexpr const char* MIN_HEIGHT_MSG =
    "Requested height: {} is less than the minimum height: {} required by the modules";

static constexpr const char* MIN_WIDTH_MSG =
    "Requested width: {} is less than the minimum width: {} required by the modules";

static constexpr const char* BAR_SIZE_MSG = "Bar configured (width: {}, height: {}) for output: {}";

static constexpr const char* SIZE_DEFINED =
    "{} size is defined in the config file so it will stay like that";

const Bar::bar_mode_map Bar::PRESET_MODES = {  //
    {"default",
     {// Special mode to hold the global bar configuration
      .layer = bar_layer::BOTTOM,
      .exclusive = true,
      .passthrough = false,
      .visible = true}},
    {"dock",
     {// Modes supported by the sway config; see man sway-bar(5)
      .layer = bar_layer::BOTTOM,
      .exclusive = true,
      .passthrough = false,
      .visible = true}},
    {"hide",
     {//
      .layer = bar_layer::TOP,
      .exclusive = false,
      .passthrough = false,
      .visible = true}},
    {"invisible",
     {//
      .layer = bar_layer::BOTTOM,
      .exclusive = false,
      .passthrough = true,
      .visible = false}},
    {"overlay",
     {//
      .layer = bar_layer::TOP,
      .exclusive = false,
      .passthrough = true,
      .visible = true}}};

const std::string_view Bar::MODE_DEFAULT = "default";
const std::string_view Bar::MODE_INVISIBLE = "invisible";
const std::string_view DEFAULT_BAR_ID = "bar-0";

/* Deserializer for enum bar_layer */
void from_json(const Json::Value& j, bar_layer& l) {
  if (j == "bottom") {
    l = bar_layer::BOTTOM;
  } else if (j == "top") {
    l = bar_layer::TOP;
  } else if (j == "overlay") {
    l = bar_layer::OVERLAY;
  }
}

/* Deserializer for struct bar_mode */
void from_json(const Json::Value& j, bar_mode& m) {
  if (j.isObject()) {
    if (auto v = j["layer"]; v.isString()) {
      from_json(v, m.layer);
    }
    if (auto v = j["exclusive"]; v.isBool()) {
      m.exclusive = v.asBool();
    }
    if (auto v = j["passthrough"]; v.isBool()) {
      m.passthrough = v.asBool();
    }
    if (auto v = j["visible"]; v.isBool()) {
      m.visible = v.asBool();
    }
  }
}

/* Deserializer for JSON Object -> map<string compatible type, Value>
 * Assumes that all the values in the object are deserializable to the same type.
 */
template <typename Key, typename Value,
          typename = std::enable_if_t<std::is_convertible<std::string_view, Key>::value>>
void from_json(const Json::Value& j, std::map<Key, Value>& m) {
  if (j.isObject()) {
    for (auto it = j.begin(); it != j.end(); ++it) {
      from_json(*it, m[it.key().asString()]);
    }
  }
}

#ifdef HAVE_GTK_LAYER_SHELL
struct GLSSurfaceImpl : public BarSurface, public sigc::trackable {
  GLSSurfaceImpl(Gtk::Window& window, struct waybar_output& output) : window_{window} {
    output_name_ = output.name;
    // this has to be executed before GtkWindow.realize
    gtk_layer_init_for_window(window_.gobj());
    gtk_layer_set_keyboard_interactivity(window.gobj(), FALSE);
    gtk_layer_set_monitor(window_.gobj(), output.monitor->gobj());
    gtk_layer_set_namespace(window_.gobj(), "waybar");

    window.signal_map_event().connect_notify(sigc::mem_fun(*this, &GLSSurfaceImpl::onMap));
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

  void setLayer(bar_layer value) override {
    auto layer = GTK_LAYER_SHELL_LAYER_BOTTOM;
    if (value == bar_layer::TOP) {
      layer = GTK_LAYER_SHELL_LAYER_TOP;
    } else if (value == bar_layer::OVERLAY) {
      layer = GTK_LAYER_SHELL_LAYER_OVERLAY;
    }
    gtk_layer_set_layer(window_.gobj(), layer);
  }

  void setPassThrough(bool enable) override {
    passthrough_ = enable;
    auto gdk_window = window_.get_window();
    if (gdk_window) {
      Cairo::RefPtr<Cairo::Region> region;
      if (enable) {
        region = Cairo::Region::create();
      }
      gdk_window->input_shape_combine_region(region, 0, 0);
    }
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
    for (auto edge : {GTK_LAYER_SHELL_EDGE_LEFT, GTK_LAYER_SHELL_EDGE_RIGHT,
                      GTK_LAYER_SHELL_EDGE_TOP, GTK_LAYER_SHELL_EDGE_BOTTOM}) {
      gtk_layer_set_anchor(window_.gobj(), edge, unanchored != edge);
    }

    // Disable anchoring for other edges too if the width
    // or the height has been set to a value other than 'auto'
    // otherwise the bar will use all space
    if (vertical_ && height_ > 1) {
      gtk_layer_set_anchor(window_.gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, false);
      gtk_layer_set_anchor(window_.gobj(), GTK_LAYER_SHELL_EDGE_TOP, false);
    } else if (!vertical_ && width_ > 1) {
      gtk_layer_set_anchor(window_.gobj(), GTK_LAYER_SHELL_EDGE_LEFT, false);
      gtk_layer_set_anchor(window_.gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, false);
    }
  }

  void setSize(uint32_t width, uint32_t height) override {
    width_ = width;
    height_ = height;
    window_.set_size_request(width_, height_);
  };

 private:
  Gtk::Window& window_;
  std::string output_name_;
  uint32_t width_;
  uint32_t height_;
  bool passthrough_ = false;
  bool vertical_ = false;

  void onMap(GdkEventAny* ev) { setPassThrough(passthrough_); }

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
      zwlr_layer_surface_v1_set_exclusive_zone(layer_surface_.get(), zone);
    }
  }

  void setLayer(bar_layer layer) override {
    layer_ = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
    if (layer == bar_layer::TOP) {
      layer_ = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
    } else if (layer == bar_layer::OVERLAY) {
      layer_ = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
    }
    // updating already mapped window
    if (layer_surface_) {
      if (zwlr_layer_surface_v1_get_version(layer_surface_.get()) >=
          ZWLR_LAYER_SURFACE_V1_SET_LAYER_SINCE_VERSION) {
        zwlr_layer_surface_v1_set_layer(layer_surface_.get(), layer_);
      } else {
        spdlog::warn("Unable to change layer: layer-shell implementation is too old");
      }
    }
  }

  void setMargins(const struct bar_margins& margins) override {
    margins_ = margins;
    // updating already mapped window
    if (layer_surface_) {
      zwlr_layer_surface_v1_set_margin(layer_surface_.get(), margins_.top, margins_.right,
                                       margins_.bottom, margins_.left);
    }
  }

  void setPassThrough(bool enable) override {
    passthrough_ = enable;
    /* GTK overwrites any region changes applied directly to the wl_surface,
     * thus the same GTK region API as in the GLS impl has to be used. */
    auto gdk_window = window_.get_window();
    if (gdk_window) {
      Cairo::RefPtr<Cairo::Region> region;
      if (enable) {
        region = Cairo::Region::create();
      }
      gdk_window->input_shape_combine_region(region, 0, 0);
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
      zwlr_layer_surface_v1_set_anchor(layer_surface_.get(), anchor_);
    }
  }

  void setSize(uint32_t width, uint32_t height) override {
    configured_width_ = width_ = width;
    configured_height_ = height_ = height;
    // layer_shell.configure handler should update exclusive zone if size changes
    window_.set_size_request(width, height);
  };

  void commit() override {
    if (surface_) {
      wl_surface_commit(surface_);
    }
  }

 private:
  constexpr static uint8_t VERTICAL_ANCHOR =
      ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
  constexpr static uint8_t HORIZONTAL_ANCHOR =
      ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;

  template <auto fn>
  using deleter_fn = std::integral_constant<decltype(fn), fn>;
  using layer_surface_ptr =
      std::unique_ptr<zwlr_layer_surface_v1, deleter_fn<zwlr_layer_surface_v1_destroy>>;

  Gtk::Window& window_;
  std::string output_name_;
  uint32_t configured_width_ = 0;
  uint32_t configured_height_ = 0;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint8_t anchor_ = HORIZONTAL_ANCHOR | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
  bool exclusive_zone_ = true;
  bool passthrough_ = false;
  struct bar_margins margins_;

  zwlr_layer_shell_v1_layer layer_ = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
  struct wl_output* output_ = nullptr;    // owned by GTK
  struct wl_surface* surface_ = nullptr;  // owned by GTK
  layer_surface_ptr layer_surface_;

  void onRealize() {
    auto gdk_window = window_.get_window()->gobj();
    gdk_wayland_window_set_use_custom_surface(gdk_window);
  }

  void onMap(GdkEventAny* ev) {
    static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
        .configure = onSurfaceConfigure,
        .closed = onSurfaceClosed,
    };
    auto client = Client::inst();
    auto gdk_window = window_.get_window()->gobj();
    surface_ = gdk_wayland_window_get_wl_surface(gdk_window);

    layer_surface_.reset(zwlr_layer_shell_v1_get_layer_surface(client->layer_shell, surface_,
                                                               output_, layer_, "waybar"));

    zwlr_layer_surface_v1_add_listener(layer_surface_.get(), &layer_surface_listener, this);
    zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface_.get(), false);
    zwlr_layer_surface_v1_set_anchor(layer_surface_.get(), anchor_);
    zwlr_layer_surface_v1_set_margin(layer_surface_.get(), margins_.top, margins_.right,
                                     margins_.bottom, margins_.left);

    setSurfaceSize(width_, height_);
    setExclusiveZone(exclusive_zone_);
    setPassThrough(passthrough_);

    commit();
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
      if (configured_height_ > 1) {
        spdlog::info(SIZE_DEFINED, "Height");
      } else {
        tmp_height = ev->height;
      }
    }
    if (ev->width > static_cast<int>(width_)) {
      // Default minimal value
      if (width_ > 1) {
        spdlog::warn(MIN_WIDTH_MSG, width_, ev->width);
      }
      if (configured_width_ > 1) {
        spdlog::info(SIZE_DEFINED, "Width");
      } else {
        tmp_width = ev->width;
      }
    }
    if (tmp_width != width_ || tmp_height != height_) {
      setSurfaceSize(tmp_width, tmp_height);
      commit();
    }
  }

  void setSurfaceSize(uint32_t width, uint32_t height) {
    /* If the client is anchored to two opposite edges, layer_surface.configure will return
     * size without margins for the axis.
     * layer_surface.set_size, however, expects size with margins for the anchored axis.
     * This is not specified by wlr-layer-shell and based on actual behavior of sway.
     *
     * If the size for unanchored axis is not set (0), change request to 1 to avoid automatic
     * assignment by the compositor.
     */
    if ((anchor_ & VERTICAL_ANCHOR) == VERTICAL_ANCHOR) {
      width = width > 0 ? width : 1;
      if (height > 1) {
        height += margins_.top + margins_.bottom;
      }
    } else {
      height = height > 0 ? height : 1;
      if (width > 1) {
        width += margins_.right + margins_.left;
      }
    }
    spdlog::debug("Set surface size {}x{} for output {}", width, height, output_name_);
    zwlr_layer_surface_v1_set_size(layer_surface_.get(), width, height);
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
      spdlog::info(BAR_SIZE_MSG, o->width_ == 1 ? "auto" : std::to_string(o->width_),
                   o->height_ == 1 ? "auto" : std::to_string(o->height_), o->output_name_);
      o->commit();
    }
    zwlr_layer_surface_v1_ack_configure(surface, serial);
  }

  static void onSurfaceClosed(void* data, struct zwlr_layer_surface_v1* /* surface */) {
    auto o = static_cast<RawSurfaceImpl*>(data);
    o->layer_surface_.reset();
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

  auto position = config["position"].asString();

  if (position == "right" || position == "left") {
    left_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    center_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    right_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    box_ = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0);
    vertical = true;
  }

  left_.get_style_context()->add_class("modules-left");
  center_.get_style_context()->add_class("modules-center");
  right_.get_style_context()->add_class("modules-right");

  if (config["spacing"].isInt()) {
    int spacing = config["spacing"].asInt();
    left_.set_spacing(spacing);
    center_.set_spacing(spacing);
    right_.set_spacing(spacing);
  }

  uint32_t height = config["height"].isUInt() ? config["height"].asUInt() : 0;
  uint32_t width = config["width"].isUInt() ? config["width"].asUInt() : 0;

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
    std::istringstream iss(config["margin"].asString());
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

  surface_impl_->setMargins(margins_);
  surface_impl_->setSize(width, height);
  // Position needs to be set after calculating the height due to the
  // GTK layer shell anchors logic relying on the dimensions of the bar.
  surface_impl_->setPosition(position);

  /* Read custom modes if available */
  if (auto modes = config.get("modes", {}); modes.isObject()) {
    from_json(modes, configured_modes);
  }

  /* Update "default" mode with the global bar options */
  from_json(config, configured_modes[MODE_DEFAULT]);

  if (auto mode = config.get("mode", {}); mode.isString()) {
    setMode(config["mode"].asString());
  } else {
    setMode(MODE_DEFAULT);
  }

  window.signal_map_event().connect_notify(sigc::mem_fun(*this, &Bar::onMap));

#if HAVE_SWAY
  if (auto ipc = config["ipc"]; ipc.isBool() && ipc.asBool()) {
    bar_id = Client::inst()->bar_id;
    if (auto id = config["id"]; id.isString()) {
      bar_id = id.asString();
    }
    if (bar_id.empty()) {
      bar_id = DEFAULT_BAR_ID;
    }
    try {
      _ipc_client = std::make_unique<BarIpcClient>(*this);
    } catch (const std::exception& exc) {
      spdlog::warn("Failed to open bar ipc connection: {}", exc.what());
    }
  }
#endif

  setupWidgets();
  window.show_all();

  if (spdlog::should_log(spdlog::level::debug)) {
    // Unfortunately, this function isn't in the C++ bindings, so we have to call the C version.
    char* gtk_tree = gtk_style_context_to_string(
        window.get_style_context()->gobj(),
        (GtkStyleContextPrintFlags)(GTK_STYLE_CONTEXT_PRINT_RECURSE |
                                    GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE));
    spdlog::debug("GTK widget tree:\n{}", gtk_tree);
    g_free(gtk_tree);
  }
}

/* Need to define it here because of forward declared members */
waybar::Bar::~Bar() = default;

void waybar::Bar::setMode(const std::string_view& mode) {
  using namespace std::literals::string_literals;

  auto style = window.get_style_context();
  /* remove styles added by previous setMode calls */
  style->remove_class("mode-"s + last_mode_);

  auto it = configured_modes.find(mode);
  if (it != configured_modes.end()) {
    last_mode_ = mode;
    style->add_class("mode-"s + last_mode_);
    setMode(it->second);
  } else {
    spdlog::warn("Unknown mode \"{}\" requested", mode);
    last_mode_ = MODE_DEFAULT;
    style->add_class("mode-"s + last_mode_);
    setMode(configured_modes.at(MODE_DEFAULT));
  }
}

void waybar::Bar::setMode(const struct bar_mode& mode) {
  surface_impl_->setLayer(mode.layer);
  surface_impl_->setExclusiveZone(mode.exclusive);
  surface_impl_->setPassThrough(mode.passthrough);

  if (mode.visible) {
    window.get_style_context()->remove_class("hidden");
    window.set_opacity(1);
  } else {
    window.get_style_context()->add_class("hidden");
    window.set_opacity(0);
  }
  surface_impl_->commit();
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
  if (auto mode = config.get("mode", {}); mode.isString()) {
    setMode(visible ? config["mode"].asString() : MODE_INVISIBLE);
  } else {
    setMode(visible ? MODE_DEFAULT : MODE_INVISIBLE);
  }
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
  for (auto& module : modules_all_) {
    auto* custom = dynamic_cast<waybar::modules::Custom*>(module.get());
    if (custom != nullptr) {
      custom->refresh(signal);
    }
  }
}

void waybar::Bar::getModules(const Factory& factory, const std::string& pos,
                             Gtk::Box* group = nullptr) {
  auto module_list = group ? config[pos]["modules"] : config[pos];
  if (module_list.isArray()) {
    for (const auto& name : module_list) {
      try {
        auto ref = name.asString();
        AModule* module;

        if (ref.compare(0, 6, "group/") == 0 && ref.size() > 6) {
          auto hash_pos = ref.find('#');
          auto id_name = ref.substr(6, hash_pos - 6);
          auto class_name = hash_pos != std::string::npos ? ref.substr(hash_pos + 1) : "";

          auto parent = group ? group : &this->box_;
          auto vertical = parent->get_orientation() == Gtk::ORIENTATION_VERTICAL;
          auto group_module = new waybar::Group(id_name, class_name, config[ref], vertical);
          getModules(factory, ref, &group_module->box);
          module = group_module;
        } else {
          module = factory.makeModule(ref);
        }

        std::shared_ptr<AModule> module_sp(module);
        modules_all_.emplace_back(module_sp);
        if (group) {
          group->pack_start(*module, false, false);
        } else {
          if (pos == "modules-left") {
            modules_left_.emplace_back(module_sp);
          }
          if (pos == "modules-center") {
            modules_center_.emplace_back(module_sp);
          }
          if (pos == "modules-right") {
            modules_right_.emplace_back(module_sp);
          }
        }
        module->dp.connect([module, ref] {
          try {
            module->update();
          } catch (const std::exception& e) {
            spdlog::error("{}: {}", ref, e.what());
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
  if (config["fixed-center"].isBool() ? config["fixed-center"].asBool() : true) {
    box_.set_center_widget(center_);
  } else {
    box_.pack_start(center_, true, false);
  }
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
