#include "bar.hpp"

#include <gdk/wayland/gdkwayland.h>
#include <gtk4-layer-shell.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <ostream>
#include <type_traits>

#include "client.hpp"
#include "factory.hpp"
#include "group.hpp"
#include "util/enum.hpp"
#include "util/hosts_check.hpp"
#include "util/kill_signal.hpp"

#ifdef HAVE_SWAY
#include "modules/sway/bar.hpp"
#endif
#ifdef HAVE_BAR_BLUR
#include "ext-background-effect-v1-client-protocol.h"
#endif

namespace waybar {
static constexpr const char* MIN_HEIGHT_MSG =
    "Requested height: {} is less than the minimum height: {} required by the modules";

static constexpr const char* MIN_WIDTH_MSG =
    "Requested width: {} is less than the minimum width: {} required by the modules";

static constexpr const char* BAR_SIZE_MSG = "Bar configured (width: {}, height: {}) for output: {}";

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
      .layer = bar_layer::OVERLAY,
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
      .layer = bar_layer::OVERLAY,
      .exclusive = false,
      .passthrough = true,
      .visible = true}}};

const std::string Bar::MODE_DEFAULT = "default";
const std::string Bar::MODE_INVISIBLE = "invisible";
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
    if (const auto& v = j["layer"]; v.isString()) {
      from_json(v, m.layer);
    }
    if (const auto& v = j["exclusive"]; v.isBool()) {
      m.exclusive = v.asBool();
    }
    if (const auto& v = j["passthrough"]; v.isBool()) {
      m.passthrough = v.asBool();
    }
    if (const auto& v = j["visible"]; v.isBool()) {
      m.visible = v.asBool();
    }
  }
}

/* Deserializer for enum Gtk::PositionType */
void from_json(const Json::Value& j, Gtk::PositionType& pos) {
  if (j == "left") {
    pos = Gtk::PositionType::LEFT;
  } else if (j == "right") {
    pos = Gtk::PositionType::RIGHT;
  } else if (j == "top") {
    pos = Gtk::PositionType::TOP;
  } else if (j == "bottom") {
    pos = Gtk::PositionType::BOTTOM;
  }
}

Glib::ustring to_string(Gtk::PositionType pos) {
  switch (pos) {
    case Gtk::PositionType::LEFT:
      return "left";
    case Gtk::PositionType::RIGHT:
      return "right";
    case Gtk::PositionType::TOP:
      return "top";
    case Gtk::PositionType::BOTTOM:
      return "bottom";
  }
  throw std::runtime_error("Invalid Gtk::PositionType");
}

/* Deserializer for JSON Object -> map<string compatible type, Value>
 * Assumes that all the values in the object are deserializable to the same type.
 */
template <typename Key, typename Value,
          typename = std::enable_if_t<std::is_convertible_v<std::string, Key>>>
void from_json(const Json::Value& j, std::map<Key, Value>& m) {
  if (j.isObject()) {
    for (auto it = j.begin(); it != j.end(); ++it) {
      from_json(*it, m[it.key().asString()]);
    }
  }
}

static void limitSize(int& minimum, int& natural, int available) {
  // A monitor whose geometry is not known yet reports 0
  if (available > 0) {
    minimum = std::min(minimum, available);
    natural = std::min(natural, available);
  }
  // A Wayland surface cannot be empty, and modules have no text before their first update
  minimum = std::max(minimum, 1);
  natural = std::max(natural, 1);
}

static GtkBorder cssInset(const Gtk::Widget& widget) {
  GtkBorder border;
  GtkBorder padding;
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  auto* context = gtk_widget_get_style_context(const_cast<GtkWidget*>(widget.gobj()));
  gtk_style_context_get_border(context, &border);
  gtk_style_context_get_padding(context, &padding);
  G_GNUC_END_IGNORE_DEPRECATIONS
  return {.left = static_cast<gint16>(border.left + padding.left),
          .right = static_cast<gint16>(border.right + padding.right),
          .top = static_cast<gint16>(border.top + padding.top),
          .bottom = static_cast<gint16>(border.bottom + padding.bottom)};
}

};  // namespace waybar

void waybar::BarWindow::measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum,
                                      int& natural, int& minimum_baseline,
                                      int& natural_baseline) const {
  // GTK adds the inset around this size, so the window ends up the size of its child
  const auto inset = cssInset(*this);
  const bool horizontal = orientation == Gtk::Orientation::HORIZONTAL;
  const int inset_size = horizontal ? inset.left + inset.right : inset.top + inset.bottom;
  const int other_inset_size = horizontal ? inset.top + inset.bottom : inset.left + inset.right;
  Gtk::Window::measure_vfunc(orientation, for_size < 0 ? for_size : for_size + other_inset_size,
                             minimum, natural, minimum_baseline, natural_baseline);
  minimum = std::max(minimum - inset_size, 0);
  natural = std::max(natural - inset_size, 0);
  Gdk::Rectangle geometry;
  monitor_->get_geometry(geometry);
  const int available = horizontal ? geometry.get_width() : geometry.get_height();
  limitSize(minimum, natural, available > inset_size ? available - inset_size : available);
}

void waybar::BarWindow::size_allocate_vfunc(int width, int height, int baseline) {
  const auto inset = cssInset(*this);
  const int child_width = width + inset.left + inset.right;
  const int child_height = height + inset.top + inset.bottom;
  Gtk::Window::size_allocate_vfunc(child_width, child_height, baseline);
  auto* child = get_child();
  if (child == nullptr || !child->get_visible()) {
    return;
  }
  // The allocations below keep each widget's size, so GTK only moves it
  if (inset.left != 0 || inset.top != 0) {
    child->size_allocate(Gdk::Rectangle(-inset.left, -inset.top, child_width, child_height), -1);
  }
  // Every resize reaches the window, so the CenterBox is never laid out without this correction
  auto* center_box = dynamic_cast<Gtk::CenterBox*>(child);
  auto* center = center_box != nullptr ? center_box->get_center_widget() : nullptr;
  if (center == nullptr || !center->get_visible()) {
    return;
  }
  const bool horizontal = center_box->get_orientation() == Gtk::Orientation::HORIZONTAL;
  const int size = horizontal ? child_width : child_height;
  GtkAllocation allocation;
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_widget_get_allocation(center->gobj(), &allocation);
  G_GNUC_END_IGNORE_DEPRECATIONS
  int& pos = horizontal ? allocation.x : allocation.y;
  const int center_size = horizontal ? allocation.width : allocation.height;
  // CenterBox puts it at size / 2 - center_size / 2, a pixel after GTK3's (size - center_size) / 2
  // when only center_size is odd. A center widget pushed aside by a side widget stays put.
  if (size % 2 == 0 && center_size % 2 != 0 && pos == size / 2 - center_size / 2) {
    --pos;
    center->size_allocate(
        Gtk::Allocation(allocation.x, allocation.y, allocation.width, allocation.height), -1);
  }
}

waybar::Bar::Bar(struct waybar_output* w_output, const Json::Value& w_config)
    : output(w_output),
      config(w_config),
      surface(nullptr),
      window{w_output->monitor},
      x_global(0),
      y_global(0),
      margins_{.top = 0, .right = 0, .bottom = 0, .left = 0},
      left_(Gtk::Orientation::HORIZONTAL, 0),
      center_(Gtk::Orientation::HORIZONTAL, 0),
      right_(Gtk::Orientation::HORIZONTAL, 0),
      box_(Gtk::Orientation::HORIZONTAL, 0) {
  window.set_title("waybar");
  window.set_name("waybar");
  window.set_decorated(false);
  for (const auto& css_class : {output->name, config["name"].asString()}) {
    if (!css_class.empty()) {
      window.add_css_class(css_class);
    }
  }

  from_json(config["position"], position);
  orientation = (position == Gtk::PositionType::LEFT || position == Gtk::PositionType::RIGHT)
                    ? Gtk::Orientation::VERTICAL
                    : Gtk::Orientation::HORIZONTAL;

  window.add_css_class(to_string(position));

  for (Gtk::Orientable* box :
       std::initializer_list<Gtk::Orientable*>{&left_, &center_, &right_, &box_, &center_box_}) {
    box->set_orientation(orientation);
  }

  left_.add_css_class("modules-left");
  center_.add_css_class("modules-center");
  right_.add_css_class("modules-right");

  if (config["spacing"].isInt()) {
    int spacing = config["spacing"].asInt();
    left_.set_spacing(spacing);
    center_.set_spacing(spacing);
    right_.set_spacing(spacing);
  }

  if (config.isMember("height") && !config["height"].isUInt()) {
    spdlog::warn("Invalid type for 'height', expected unsigned integer");
  }

  height_ = config["height"].isUInt() ? config["height"].asUInt() : 0;
  width_ = config["width"].isUInt() ? config["width"].asUInt() : 0;

#ifdef HAVE_BAR_BLUR
  blur_enabled_ = config["blur"].isBool() && config["blur"].asBool();
#endif

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

  output->monitor->property_geometry().signal_changed().connect(
      sigc::mem_fun(*this, &Bar::onOutputGeometryChanged));

  // this has to be executed before GtkWindow.realize
  auto* gtk_window = window.gobj();
  gtk_layer_init_for_window(gtk_window);
  gtk_layer_set_keyboard_mode(gtk_window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
  gtk_layer_set_monitor(gtk_window, output->monitor->gobj());
  gtk_layer_set_namespace(gtk_window,
                          config["name"].isString() ? config["name"].asCString() : "waybar");

  gtk_layer_set_margin(gtk_window, GTK_LAYER_SHELL_EDGE_LEFT, margins_.left);
  gtk_layer_set_margin(gtk_window, GTK_LAYER_SHELL_EDGE_RIGHT, margins_.right);
  gtk_layer_set_margin(gtk_window, GTK_LAYER_SHELL_EDGE_TOP, margins_.top);
  gtk_layer_set_margin(gtk_window, GTK_LAYER_SHELL_EDGE_BOTTOM, margins_.bottom);

  window.set_size_request(width_, height_);

  // Position needs to be set after calculating the height due to the
  // GTK layer shell anchors logic relying on the dimensions of the bar.
  setPosition(position);

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

  if (config["start_hidden"].asBool()) {
    setVisible(false);
  }

  unmap_conn_ = window.signal_unmap().connect([this]() {
    spdlog::debug("Output {} unmapped, suspending modules", output->name);
    layout_conn_.disconnect();
    compute_size_conn_.disconnect();
    surface = nullptr;  // GTK destroys the wl_surface; onMap() fetches the new one
    toggleSuspend(true);
  });

  // `map` is emitted synchronously from present(), so modules have the surface in their first
  // update().
  map_conn_ = window.signal_map().connect([this]() {
    spdlog::debug("Output {} mapped, resuming modules", output->name);
    onMap();
    toggleSuspend(false);
  });

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

  const auto& configSigusr1 = config["on-sigusr1"];
  if (configSigusr1.isString()) {
    auto strSigusr1 = configSigusr1.asString();
    try {
      onSigusr1 =
          util::parseStringToEnum<util::KillSignalAction>(strSigusr1, util::userKillSignalActions);
    } catch (const std::invalid_argument& e) {
      onSigusr1 = util::SIGNALACTION_DEFAULT_SIGUSR1;
      spdlog::warn(
          "Invalid string representation for on-sigusr1. Falling back to default mode (toggle).");
    }
  }
  const auto& configSigusr2 = config["on-sigusr2"];
  if (configSigusr2.isString()) {
    auto strSigusr2 = configSigusr2.asString();
    try {
      onSigusr2 =
          util::parseStringToEnum<util::KillSignalAction>(strSigusr2, util::userKillSignalActions);
    } catch (const std::invalid_argument& e) {
      onSigusr2 = util::SIGNALACTION_DEFAULT_SIGUSR2;
      spdlog::warn(
          "Invalid string representation for on-sigusr2. Falling back to default mode (reload).");
    }
  }

  setupWidgets();
  window.present();

  // The first update of every module, in one source: a dispatcher emit per module before the
  // main loop runs fills the pipe and blocks the main thread with enough modules.
  Glib::signal_idle().connect(sigc::track_obj(
                                  [this] {
                                    for (auto const& module : modules_all_) {
                                      module->dp.dispatchPending();
                                    }
                                    return false;
                                  },
                                  *this),
                              Glib::PRIORITY_DEFAULT);

  if (spdlog::should_log(spdlog::level::debug)) {
    // Unfortunately, this function isn't in the C++ bindings, so we have to call the C version.
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    char* gtk_tree = gtk_style_context_to_string(
        gtk_widget_get_style_context(GTK_WIDGET(window.gobj())),
        (GtkStyleContextPrintFlags)(GTK_STYLE_CONTEXT_PRINT_RECURSE |
                                    GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE));
    G_GNUC_END_IGNORE_DEPRECATIONS
    spdlog::debug("GTK widget tree:\n{}", gtk_tree);
    g_free(gtk_tree);
  }
}

/* Need to define it here because of forward declared members */
waybar::Bar::~Bar() {
  /* Destroying the window emits `unmap`, whose handler runs toggleSuspend() over
   * modules_all_ -- already freed by this point. Disconnect first (#5182). */
  unmap_conn_.disconnect();
  map_conn_.disconnect();
  layout_conn_.disconnect();
  compute_size_conn_.disconnect();
#ifdef HAVE_BAR_BLUR
  if (background_effect_surface_) {
    ext_background_effect_surface_v1_destroy(background_effect_surface_);
    background_effect_surface_ = nullptr;
  }
#endif
}

void waybar::Bar::setMode(const std::string& mode) {
  using namespace std::literals::string_literals;

  /* remove styles added by previous setMode calls */
  window.remove_css_class("mode-"s + last_mode_);

  auto it = configured_modes.find(mode);
  if (it != configured_modes.end()) {
    last_mode_ = mode;
    window.add_css_class("mode-"s + last_mode_);
    setMode(it->second);
  } else {
    spdlog::warn("Unknown mode \"{}\" requested", mode);
    last_mode_ = MODE_DEFAULT;
    window.add_css_class("mode-"s + last_mode_);
    setMode(configured_modes.at(MODE_DEFAULT));
  }
}

void waybar::Bar::setMode(const struct bar_mode& mode) {
  auto* gtk_window = window.gobj();

  auto layer = GTK_LAYER_SHELL_LAYER_BOTTOM;
  if (mode.layer == bar_layer::TOP) {
    layer = GTK_LAYER_SHELL_LAYER_TOP;
  } else if (mode.layer == bar_layer::OVERLAY) {
    layer = GTK_LAYER_SHELL_LAYER_OVERLAY;
  }
  gtk_layer_set_layer(gtk_window, layer);

  if (mode.exclusive) {
    gtk_layer_auto_exclusive_zone_enable(gtk_window);
  } else {
    gtk_layer_set_exclusive_zone(gtk_window, 0);
  }

  setPassThrough(passthrough_ = mode.passthrough);

  if (mode.visible) {
    window.remove_css_class("hidden");
    window.set_opacity(1);
  } else {
    window.add_css_class("hidden");
    window.set_opacity(0);
  }
  /*
   * All the changes above require `wl_surface_commit`.
   * gtk-layer-shell schedules a commit on the next frame event in GTK, but this could fail in
   * certain scenarios, such as fully occluded bar.
   */
  forceLayerCommit();
}

void waybar::Bar::forceLayerCommit() {
  /*
   * Mode changes are double-buffered layer-surface state, which GTK only commits with its next
   * frame. A fully occluded bar gets no frame callback, so commit them now. GTK sets the buffer
   * scale together with the buffer it attaches, so nothing half-done is committed.
   */
  if (surface != nullptr) {
    wl_surface_commit(surface);
  }
  wl_display_flush(Client::inst()->wl_display);
}

void waybar::Bar::setPassThrough(bool passthrough) {
  if (auto gdk_surface = window.get_surface()) {
    // An empty region lets every input through, NULL restores the default (the whole surface)
    cairo_region_t* region = passthrough ? cairo_region_create() : nullptr;
    gdk_surface_set_input_region(gdk_surface->gobj(), region);
    if (region != nullptr) {
      cairo_region_destroy(region);
    }
  }
}

void waybar::Bar::setPosition(Gtk::PositionType position) {
  std::array<gboolean, GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER> anchors;
  anchors.fill(TRUE);

  auto orientation = (position == Gtk::PositionType::LEFT || position == Gtk::PositionType::RIGHT)
                         ? Gtk::Orientation::VERTICAL
                         : Gtk::Orientation::HORIZONTAL;

  switch (position) {
    case Gtk::PositionType::LEFT:
      anchors[GTK_LAYER_SHELL_EDGE_RIGHT] = FALSE;
      break;
    case Gtk::PositionType::RIGHT:
      anchors[GTK_LAYER_SHELL_EDGE_LEFT] = FALSE;
      break;
    case Gtk::PositionType::BOTTOM:
      anchors[GTK_LAYER_SHELL_EDGE_TOP] = FALSE;
      break;
    default: /* Gtk::PositionType::TOP */
      anchors[GTK_LAYER_SHELL_EDGE_BOTTOM] = FALSE;
      break;
  };
  // Disable anchoring for other edges too if the width
  // or the height has been set to a value other than 'auto'
  // otherwise the bar will use all space
  uint32_t configured_width = config["width"].isUInt() ? config["width"].asUInt() : 0;
  uint32_t configured_height = config["height"].isUInt() ? config["height"].asUInt() : 0;
  if (orientation == Gtk::Orientation::VERTICAL && configured_height > 1) {
    anchors[GTK_LAYER_SHELL_EDGE_TOP] = FALSE;
    anchors[GTK_LAYER_SHELL_EDGE_BOTTOM] = FALSE;
  } else if (orientation == Gtk::Orientation::HORIZONTAL && configured_width > 1) {
    anchors[GTK_LAYER_SHELL_EDGE_LEFT] = FALSE;
    anchors[GTK_LAYER_SHELL_EDGE_RIGHT] = FALSE;
  }

  for (auto edge : {GTK_LAYER_SHELL_EDGE_LEFT, GTK_LAYER_SHELL_EDGE_RIGHT, GTK_LAYER_SHELL_EDGE_TOP,
                    GTK_LAYER_SHELL_EDGE_BOTTOM}) {
    gtk_layer_set_anchor(window.gobj(), edge, anchors[edge]);
  }
}

void waybar::Bar::onMap() {
  /*
   * Obtain a pointer to the custom layer surface for modules that require it (idle_inhibitor).
   */
  auto gdk_surface = window.get_surface();
  if (!gdk_surface) {
    spdlog::warn("Failed to get GDK surface during onMap, deferring surface initialization");
    return;
  }

  surface = gdk_wayland_surface_get_wl_surface(gdk_surface->gobj());
  layout_conn_.disconnect();
  layout_conn_ = gdk_surface->signal_layout().connect(sigc::mem_fun(*this, &Bar::onLayout));
  // GtkWindow remembers the largest size it ever had as its default size, which gtk4-layer-shell
  // requests: like GTK3, follow the natural size of the modules on the axis the compositor does
  // not stretch, so the bar shrinks back with them.
  compute_size_conn_.disconnect();
  if (auto toplevel = std::dynamic_pointer_cast<Gdk::Toplevel>(gdk_surface)) {
    compute_size_conn_ = toplevel->signal_compute_size().connect(
        [this](Gdk::ToplevelSize& /*size*/) {
          int width = 0;
          int height = 0;
          window.get_default_size(width, height);
          int minimum = 0;
          int natural = 0;
          int baseline = 0;
          const bool vertical = orientation == Gtk::Orientation::VERTICAL;
          window.measure(vertical ? Gtk::Orientation::HORIZONTAL : Gtk::Orientation::VERTICAL,
                         vertical || width <= 0 ? -1 : width, minimum, natural, baseline, baseline);
          if (natural != (vertical ? width : height)) {
            window.set_default_size(vertical ? natural : width, vertical ? height : natural);
          }
        },
        true);
  }
  configureGlobalOffset(gdk_surface->get_width(), gdk_surface->get_height());

  setPassThrough(passthrough_);
#ifdef HAVE_BAR_BLUR
  setupBackgroundBlur();
#endif
}

#ifdef HAVE_BAR_BLUR
// Requests blur-behind for this bar's own surface, via the standard
// ext-background-effect-v1 protocol.
void waybar::Bar::setupBackgroundBlur() {
  if (!blur_enabled_ || background_effect_surface_ || !surface) {
    return;
  }
  auto* manager = Client::inst()->background_effect_manager;
  if (!manager) {
    spdlog::warn("[bar]: Compositor does not support ext-background-effect-v1; blur disabled");
    return;
  }
  background_effect_surface_ =
      ext_background_effect_manager_v1_get_background_effect(manager, surface);
  updateBlurRegion();
}

// Re-applies the blur region to match the bar's current pixel size, clipped to the bar's
// actual rendered CSS background shape (rounded corners, including asymmetric top/bottom
// radii) so blur doesn't leak past it as a rectangular "ghost".
void waybar::Bar::updateBlurRegion() {
  if (!blur_enabled_ || !background_effect_surface_ || !surface) {
    return;
  }
  // Query the live surface size rather than width_/height_: those track the
  // user-requested config size and may still be unset (0) the first time we're
  // called, from onMap, before any layout has landed.
  auto gdk_surface = window.get_surface();
  if (!gdk_surface) {
    return;
  }
  const int32_t w = gdk_surface->get_width();
  const int32_t h = gdk_surface->get_height();
  if (w <= 0 || h <= 0) {
    return;
  }
  auto* compositor = gdk_wayland_display_get_wl_compositor(window.get_display()->gobj());
  if (!compositor) {
    return;
  }

  // Rasterize the actual CSS background to find each row's opaque span, so rounded
  // corners are clipped correctly even when the top and bottom radii differ.
  cairo_surface_t* mask = cairo_image_surface_create(CAIRO_FORMAT_A8, w, h);
  cairo_t* cr = cairo_create(mask);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_render_background(gtk_widget_get_style_context(GTK_WIDGET(window.gobj())), cr, 0, 0, w, h);
  G_GNUC_END_IGNORE_DEPRECATIONS
  cairo_surface_flush(mask);
  const unsigned char* data = cairo_image_surface_get_data(mask);
  const int stride = cairo_image_surface_get_stride(mask);

  wl_region* region = wl_compositor_create_region(compositor);
  // Rows are added as runs of identical spans, a single rectangle without rounded corners.
  int32_t run_start = 0;
  int32_t run_left = 0;
  int32_t run_right = 0;
  auto flush_run = [&](int32_t end) {
    if (run_right > run_left && end > run_start) {
      wl_region_add(region, run_left, run_start, run_right - run_left, end - run_start);
    }
  };
  for (int32_t y = 0; y < h; ++y) {
    const unsigned char* row = data + static_cast<size_t>(y) * stride;
    int32_t left = 0;
    while (left < w && row[left] == 0) ++left;
    int32_t right = w;
    while (right > left && row[right - 1] == 0) --right;
    if (y == 0 || left != run_left || right != run_right) {
      flush_run(y);
      run_start = y;
      run_left = left;
      run_right = right;
    }
  }
  flush_run(h);
  cairo_destroy(cr);
  cairo_surface_destroy(mask);
  ext_background_effect_surface_v1_set_blur_region(background_effect_surface_, region);
  wl_region_destroy(region);
  forceLayerCommit();
}
#endif

void waybar::Bar::setVisible(bool value) {
  visible = value;
  if (auto mode = config.get("mode", {}); mode.isString()) {
    setMode(visible ? config["mode"].asString() : MODE_INVISIBLE);
  } else {
    setMode(visible ? MODE_DEFAULT : MODE_INVISIBLE);
  }
}

void waybar::Bar::toggle() { setVisible(!visible); }
void waybar::Bar::show() { setVisible(true); }
void waybar::Bar::hide() { setVisible(false); }

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
        auto ref = module_name.asString();
        if (ref.compare(0, 6, "group/") == 0 && ref.size() > 6) {
          Json::Value& group_modules = config[ref]["modules"];
          for (const Json::Value& module_name : group_modules) {
            if (module_name.isString()) {
              setupAltFormatKeyForModule(module_name.asString());
            }
          }
        } else {
          setupAltFormatKeyForModule(ref);
        }
      }
    }
  }
}

void waybar::Bar::handleSignal(int signal) {
  for (auto& module : modules_all_) {
    module->refresh(signal);
  }
}

waybar::util::KillSignalAction waybar::Bar::getOnSigusr1Action() { return this->onSigusr1; }
waybar::util::KillSignalAction waybar::Bar::getOnSigusr2Action() { return this->onSigusr2; }

void waybar::Bar::getModules(const Factory& factory, const std::string& pos,
                             waybar::Group* group = nullptr) {
  auto module_list = group != nullptr ? config[pos]["modules"] : config[pos];
  if (module_list.isArray()) {
    for (const auto& name : module_list) {
      try {
        auto ref = name.asString();

        if (config[ref].isMember("hosts") && !waybar::util::valid_host(config[ref])) {
          continue;
        }

        AModule* module;

        if (ref.compare(0, 6, "group/") == 0 && ref.size() > 6) {
          auto hash_pos = ref.find('#');
          auto id_name = ref.substr(6, hash_pos - 6);
          auto class_name = hash_pos != std::string::npos ? ref.substr(hash_pos + 1) : "";

          auto vertical = (group != nullptr ? group->getBox().get_orientation() : orientation) ==
                          Gtk::Orientation::VERTICAL;

          const Json::Value& group_config = config[ref];
          if (group_config["modules"].isNull()) {
            spdlog::warn("Group definition '{}' has not been found, group will be hidden", ref);
          }
          auto group_module =
              std::make_unique<waybar::Group>(id_name, class_name, group_config, vertical);

          getModules(factory, ref, group_module.get());
          module = group_module.release();
        } else {
          module = factory.makeModule(ref, pos);
        }

        std::shared_ptr<AModule> module_sp(module);
        modules_all_.emplace_back(module_sp);
        if (group != nullptr) {
          group->addWidget(module);
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
  bool expand_left = config["expand-left"].isBool() ? config["expand-left"].asBool() : false;
  bool expand_center = config["expand-center"].isBool() ? config["expand-center"].asBool() : false;
  bool expand_right = config["expand-right"].isBool() ? config["expand-right"].asBool() : false;
  bool no_center = config["no-center"].isBool() ? config["no-center"].asBool() : false;
  bool fixed_center =
      !no_center && (config["fixed-center"].isBool() ? config["fixed-center"].asBool() : true);
  const bool horizontal = orientation == Gtk::Orientation::HORIZONTAL;

  // Like GTK3 box packing: an expanding child gets a share of the free space. Expansion is always
  // set explicitly, else a module asking for it would make every ancestor expand too.
  auto set_expand = [horizontal](Gtk::Widget& widget, bool expand) {
    if (horizontal) {
      widget.set_hexpand(expand);
    } else {
      widget.set_vexpand(expand);
    }
  };
  auto set_align = [horizontal](Gtk::Widget& widget, Gtk::Align align) {
    if (horizontal) {
      widget.set_halign(align);
    } else {
      widget.set_valign(align);
    }
  };

  set_expand(left_, expand_left);
  set_expand(right_, expand_right);
  if (fixed_center) {
    set_expand(center_, expand_center);
    center_box_.set_start_widget(left_);
    center_box_.set_center_widget(center_);
    center_box_.set_end_widget(right_);
    window.set_child(center_box_);
  } else {
    box_.append(left_);
    if (!no_center) {
      // GTK3 packed it expanding, filling the free space only with expand-center
      set_expand(center_, true);
      set_align(center_, expand_center ? Gtk::Align::FILL : Gtk::Align::CENTER);
      box_.append(center_);
    } else if (!expand_right) {
      // Packed at the end: the free space goes before it
      set_expand(right_, !expand_left);
      set_align(right_, Gtk::Align::END);
    }
    box_.append(right_);
    window.set_child(box_);
  }

  // Convert to button code for every module that is used.
  setupAltFormatKeyForModuleList("modules-left");
  setupAltFormatKeyForModuleList("modules-right");
  setupAltFormatKeyForModuleList("modules-center");

  Factory factory(*this, config);
  getModules(factory, "modules-left");
  if (!no_center) {
    getModules(factory, "modules-center");
  }
  getModules(factory, "modules-right");

  auto pack = [&](Gtk::Box& box, const std::vector<std::shared_ptr<AModule>>& modules) {
    for (auto const& module : modules) {
      Gtk::Widget& widget = *module;
      set_expand(widget, module->expandEnabled());
      box.append(widget);
    }
  };
  pack(left_, modules_left_);
  if (!no_center) {
    pack(center_, modules_center_);
  }
  pack(right_, modules_right_);
  // GTK3 packed the right modules at the end of their box: give the free space of an expanding
  // box to the first module, drawn at the end of it.
  if (expand_right && !modules_right_.empty() &&
      std::none_of(modules_right_.begin(), modules_right_.end(),
                   [](const auto& module) { return module->expandEnabled(); })) {
    Gtk::Widget& first = *modules_right_.front();
    set_expand(first, true);
    set_align(first, Gtk::Align::END);
  }
}

void waybar::Bar::onLayout(int width, int height) {
  /*
   * GTK got a new size for the window.
   * Actual resizing and management of the exclusive zone is handled within the gtk4-layer-shell
   * code. This only updates the stored size of the window and prints some warnings.
   */
  if (orientation == Gtk::Orientation::VERTICAL) {
    if (width_ > 1 && width > static_cast<int>(width_)) {
      spdlog::warn(MIN_WIDTH_MSG, width_, width);
    }
  } else {
    if (height_ > 1 && height > static_cast<int>(height_)) {
      spdlog::warn(MIN_HEIGHT_MSG, height_, height);
    }
  }
  if (width_ == static_cast<uint32_t>(width) && height_ == static_cast<uint32_t>(height)) {
    return;
  }
  width_ = width;
  height_ = height;

  configureGlobalOffset(width, height);
  spdlog::info(BAR_SIZE_MSG, width, height, output->name);
#ifdef HAVE_BAR_BLUR
  updateBlurRegion();
#endif
}

void waybar::Bar::configureGlobalOffset(int width, int height) {
  Gdk::Rectangle geometry;
  output->monitor->get_geometry(geometry);
  const auto monitor_geometry = *geometry.gobj();
  int x;
  int y;
  switch (position) {
    case Gtk::PositionType::BOTTOM:
      if (width + margins_.left + margins_.right >= monitor_geometry.width)
        x = margins_.left;
      else
        x = (monitor_geometry.width - width) / 2;
      y = monitor_geometry.height - height - margins_.bottom;
      break;
    case Gtk::PositionType::LEFT:
      x = margins_.left;
      if (height + margins_.top + margins_.bottom >= monitor_geometry.height)
        y = margins_.top;
      else
        y = (monitor_geometry.height - height) / 2;
      break;
    case Gtk::PositionType::RIGHT:
      x = monitor_geometry.width - width - margins_.right;
      if (height + margins_.top + margins_.bottom >= monitor_geometry.height)
        y = margins_.top;
      else
        y = (monitor_geometry.height - height) / 2;
      break;
    default: /* Gtk::PositionType::TOP */
      if (width + margins_.left + margins_.right >= monitor_geometry.width)
        x = margins_.left;
      else
        x = (monitor_geometry.width - width) / 2;
      y = margins_.top;
      break;
  }

  x_global = x + monitor_geometry.x;
  y_global = y + monitor_geometry.y;
}

void waybar::Bar::onOutputGeometryChanged() {
  configureGlobalOffset(window.get_width(), window.get_height());
  window.queue_resize();
}

void waybar::Bar::toggleSuspend(bool suspend) {
  // Iterate the actual module objects. Modules are packed into the Gtk::Box via
  // AModule::operator Gtk::Widget&(), which returns the member event_box_, so the
  // box children are Gtk::EventBox, never AModule -- a dynamic_cast over them is
  // always null and suspend()/resume() would never fire. modules_all_ holds the
  // real module pointers (including group children), so use it instead.
  for (auto const& module : modules_all_) {
    if (module && module->shouldSuspend()) {
      if (suspend) {
        module->suspend();
      } else {
        module->resume();
      }
    }
  }
}
