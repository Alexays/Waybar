#include "bar.hpp"

#include <gtk4-layer-shell.h>
#include <spdlog/spdlog.h>

#include "client.hpp"
#include "factory.hpp"

#ifdef HAVE_SWAY
#include "modules/sway/bar.hpp"
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
    default:
      return "";
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

};  // namespace waybar

waybar::Bar::Bar(struct waybar_output* w_output, const Json::Value& w_config)
    : output{w_output},
      config{w_config},
      surface{nullptr},
      window{Gtk::Window()},
      x_global{0},
      y_global{0},
      margins_{.top = 0, .right = 0, .bottom = 0, .left = 0},
      left_{Gtk::Orientation::HORIZONTAL, 0},
      center_{Gtk::Orientation::HORIZONTAL, 0},
      right_{Gtk::Orientation::HORIZONTAL, 0},
      box_{} {
  window.set_title("waybar");
  window.set_name("waybar");
  window.set_decorated(false);
  window.set_child(box_);
  window.get_style_context()->add_class(output->name);
  window.get_style_context()->add_class(config["name"].asString());

  from_json(config["position"], position);
  orientation = (position == Gtk::PositionType::LEFT || position == Gtk::PositionType::RIGHT)
                    ? Gtk::Orientation::VERTICAL
                    : Gtk::Orientation::HORIZONTAL;

  window.get_style_context()->add_class(to_string(position));

  left_.set_orientation(orientation);
  center_.set_orientation(orientation);
  right_.set_orientation(orientation);
  box_.set_orientation(orientation);

  left_.get_style_context()->add_class("modules-left");
  center_.get_style_context()->add_class("modules-center");
  right_.get_style_context()->add_class("modules-right");

  if (config["spacing"].isInt()) {
    int spacing = config["spacing"].asInt();
    left_.set_spacing(spacing);
    center_.set_spacing(spacing);
    right_.set_spacing(spacing);
  }

  height_ = config["height"].isUInt() ? config["height"].asUInt() : 0;
  width_ = config["width"].isUInt() ? config["width"].asUInt() : 0;

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
  auto* gtk_window{window.gobj()};
  gtk_layer_init_for_window(gtk_window);
  gtk_layer_set_keyboard_mode(gtk_window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
  gtk_layer_set_monitor(gtk_window, output->monitor->gobj());
  gtk_layer_set_namespace(gtk_window, "waybar");

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

  window.signal_map().connect(sigc::mem_fun(*this, &Bar::onMap));

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
  window.show();

  if (spdlog::should_log(spdlog::level::debug)) {
    auto gtk_tree{window.get_style_context()->to_string(Gtk::StyleContext::PrintFlags::RECURSE |
                                                        Gtk::StyleContext::PrintFlags::SHOW_STYLE)};
    spdlog::debug("GTK widget tree:\n{}", gtk_tree.c_str());
  }
}

/* Need to define it here because of forward declared members */
waybar::Bar::~Bar() = default;

void waybar::Bar::setMode(const std::string& mode) {
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
    window.get_style_context()->remove_class("hidden");
    window.set_opacity(1);
  } else {
    window.get_style_context()->add_class("hidden");
    window.set_opacity(0);
  }
  /*
   * All the changes above require `wl_surface_commit`.
   * gtk-layer-shell schedules a commit on the next frame event in GTK, but this could fail in
   * certain scenarios, such as fully occluded bar.
   */
  wl_display_flush(Client::inst()->wl_display);
}

void waybar::Bar::setPassThrough(bool passthrough) {
  if (passthrough && gdk_surface_) {
    auto region{Cairo::Region::create()};
    gdk_surface_->set_input_region(region);
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
    default: /* Gtk::POS_TOP */
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
  gdk_surface_ = window.get_surface();
  gdk_surface_->signal_layout().connect(sigc::mem_fun(*this, &Bar::onConfigure));
  surface = gdk_wayland_surface_get_wl_surface(gdk_surface_->gobj());
  configureGlobalOffset(gdk_surface_->get_width(), gdk_surface_->get_height());
  setPassThrough(passthrough_);
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

void waybar::Bar::getModules(const Factory& factory, const std::string& pos,
                             waybar::Group* group = nullptr) {
  auto module_list = group != nullptr ? config[pos]["modules"] : config[pos];
  if (module_list.isArray()) {
    for (const auto& name : module_list) {
      try {
        auto ref = name.asString();
        AModule* module;

        if (ref.compare(0, 6, "group/") == 0 && ref.size() > 6) {
          auto hash_pos = ref.find('#');
          auto id_name = ref.substr(6, hash_pos - 6);
          auto class_name = hash_pos != std::string::npos ? ref.substr(hash_pos + 1) : "";

          auto vertical = (group ? group->getBox().get_orientation() : box_.get_orientation()) ==
                          Gtk::Orientation::VERTICAL;

          auto* group_module = new waybar::Group(id_name, class_name, config[ref], vertical);
          getModules(factory, ref, group_module);
          module = group_module;
        } else {
          module = factory.makeModule(ref, pos);
        }

        std::shared_ptr<AModule> module_sp(module);
        modules_all_.emplace_back(module_sp);
        if (group != nullptr) {
          group->addWidget(*module);
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
  box_.set_start_widget(left_);
  if (config["fixed-center"].isBool() ? config["fixed-center"].asBool() : true) {
    box_.set_center_widget(center_);
  } else {
    box_.set_start_widget(center_);
  }
  box_.set_end_widget(right_);
  // Convert to button code for every module that is used.
  setupAltFormatKeyForModuleList("modules-left");
  setupAltFormatKeyForModuleList("modules-right");
  setupAltFormatKeyForModuleList("modules-center");

  Factory factory(*this, config);
  getModules(factory, "modules-left");
  getModules(factory, "modules-center");
  getModules(factory, "modules-right");
  for (auto const& module : modules_left_) {
    left_.append(*module);
  }
  for (auto const& module : modules_center_) {
    center_.append(*module);
  }
  for (auto const& module : modules_right_) {
    right_.append(*module);
  }
}

void waybar::Bar::onConfigure(int width, int height) {
  /*
   * GTK wants new size for the window.
   * Actual resizing and management of the exclusve zone is handled within the gtk-layer-shell
   * code. This event handler only updates stored size of the window and prints some warnings.
   *
   * Note: forced resizing to a window smaller than required by GTK would not work with
   * gtk-layer-shell.
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
  width_ = width;
  height_ = height;

  configureGlobalOffset(width, height);
  spdlog::info(BAR_SIZE_MSG, width, height, output->name);
}

void waybar::Bar::configureGlobalOffset(int width, int height) {
  auto monitor_geometry = *output->monitor->property_geometry().get_value().gobj();
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
}
