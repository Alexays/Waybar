#include "modules/sni/item.hpp"

#include <glibmm/main.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>

#include "modules/sni/host.hpp"
#include "modules/sni/icon_manager.hpp"
#include "util/format.hpp"  // IWYU pragma: keep
#include "util/gtk/gtk_icon.hpp"

template <>
struct fmt::formatter<Glib::VariantBase> : formatter<std::string> {
  bool is_printable(const Glib::VariantBase& value) const {
    auto type = value.get_type_string();
    /* Print only primitive (single character excluding 'v') and short complex types */
    return (type.length() == 1 && islower(type[0]) && type[0] != 'v') || value.get_size() <= 32;
  }

  template <typename FormatContext>
  auto format(const Glib::VariantBase& value, FormatContext& ctx) const {
    if (is_printable(value)) {
      return formatter<std::string>::format(static_cast<std::string>(value.print()), ctx);
    } else {
      return formatter<std::string>::format(value.get_type_string(), ctx);
    }
  }
};

namespace waybar::modules::SNI {

static const Glib::ustring SNI_INTERFACE_NAME = sn_item_interface_info()->name;
static const unsigned UPDATE_DEBOUNCE_TIME = 10;

Item::Item(const std::string& bn, const std::string& op, const Json::Value& config, const Bar& bar,
           const std::function<void(Item&)>& on_ready,
           const std::function<void(Item&)>& on_invalidate, const std::function<void()>& on_updated,
           Host& host, const ItemOrderMap& orders)
    : bus_name(bn),
      object_path(op),
      widget_(Gtk::Orientation::HORIZONTAL, 0),
      icon_size(16),
      effective_icon_size(0),
      icon_theme(Gtk::IconTheme::create()),
      bar_(bar),
      on_ready_(on_ready),
      on_invalidate_(on_invalidate),
      on_updated_(on_updated),
      host_(host),
      orders_(orders) {
  if (config["icon-size"].isUInt()) {
    icon_size = config["icon-size"].asUInt();
  }
  if (config["smooth-scrolling-threshold"].isNumeric()) {
    scroll_threshold_ = config["smooth-scrolling-threshold"].asDouble();
  }
  if (config["show-passive-items"].isBool()) {
    show_passive_ = config["show-passive-items"].asBool();
  }

  widget_.append(image);
  widget_.set_visible(show_passive_);
  widget_.show();

  gesture_click_ = Gtk::GestureClick::create();
  gesture_click_->set_button(0);  // 0 == any button, ensure we catch right-click
  gesture_click_->signal_pressed().connect(sigc::mem_fun(*this, &Item::handleClick));
  widget_.add_controller(gesture_click_);

  scroll_controller_ = Gtk::EventControllerScroll::create();
  scroll_controller_->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
  scroll_controller_->signal_scroll().connect(sigc::mem_fun(*this, &Item::handleScroll), false);
  widget_.add_controller(scroll_controller_);

  motion_controller_ = Gtk::EventControllerMotion::create();
  motion_controller_->signal_enter().connect(sigc::mem_fun(*this, &Item::handleMouseEnter));
  motion_controller_->signal_leave().connect(sigc::mem_fun(*this, &Item::handleMouseLeave));
  widget_.add_controller(motion_controller_);

  cancellable_ = Gio::Cancellable::create();

  auto interface = Glib::wrap(sn_item_interface_info(), true);
  Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::SESSION, bus_name, object_path,
                                   SNI_INTERFACE_NAME, sigc::mem_fun(*this, &Item::proxyReady),
                                   cancellable_, interface);
}

Item::~Item() = default;

bool Item::isReady() const { return ready_; }

void Item::handleMouseEnter(double x, double y) {
  widget_.set_state_flags(Gtk::StateFlags::PRELIGHT);
}

void Item::handleMouseLeave() { widget_.unset_state_flags(Gtk::StateFlags::PRELIGHT); }

void Item::proxyReady(Glib::RefPtr<Gio::AsyncResult>& result) {
  try {
    this->proxy_ = Gio::DBus::Proxy::create_for_bus_finish(result);
    /* Properties are already cached during object creation */
    auto cached_properties = this->proxy_->get_cached_property_names();
    for (const auto& name : cached_properties) {
      Glib::VariantBase value;
      this->proxy_->get_cached_property(value, name);
      setProperty(name, value);
    }

    this->proxy_->signal_signal().connect(sigc::mem_fun(*this, &Item::onSignal));

    if (this->id.empty() || this->category.empty()) {
      spdlog::error("Invalid Status Notifier Item: {}, {}", bus_name, object_path);
      invalidate();
      return;
    }
    this->updateImage();
    setReady();

  } catch (const Glib::Error& err) {
    spdlog::error("Failed to create DBus Proxy for {} {}: {}", bus_name, object_path, err.what());
    invalidate();
  } catch (const std::exception& err) {
    spdlog::error("Failed to create DBus Proxy for {} {}: {}", bus_name, object_path, err.what());
    invalidate();
  }
}

template <typename T>
T get_variant(const Glib::VariantBase& value) {
  return Glib::VariantBase::cast_dynamic<Glib::Variant<T>>(value).get();
}

template <>
ToolTip get_variant<ToolTip>(const Glib::VariantBase& value) {
  ToolTip result;
  // Unwrap (sa(iiay)ss)
  auto container = value.cast_dynamic<Glib::VariantContainerBase>(value);
  result.icon_name = get_variant<Glib::ustring>(container.get_child(0));
  result.text = get_variant<Glib::ustring>(container.get_child(2));
  auto description = get_variant<Glib::ustring>(container.get_child(3));
  if (!description.empty()) {
    auto escapedDescription = Glib::Markup::escape_text(description);
    result.text = fmt::format("<b>{}</b>\n{}", result.text, escapedDescription);
  }
  return result;
}

void Item::setupDbusMenu() {
  if (menu.empty() || menu_proxy_) return;
  Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::SESSION, bus_name, menu,
                                   "com.canonical.dbusmenu",
                                   sigc::mem_fun(*this, &Item::onMenuProxyReady), cancellable_);
}

void Item::onMenuProxyReady(Glib::RefPtr<Gio::AsyncResult>& result) {
  try {
    menu_proxy_ = Gio::DBus::Proxy::create_for_bus_finish(result);
    menu_proxy_->signal_signal().connect(sigc::mem_fun(*this, &Item::onMenuSignal));
    refreshMenuLayout();
  } catch (const Glib::Error& err) {
    spdlog::warn("Failed to create DBusMenu proxy for {}: {}", bus_name, err.what());
  }
}

void Item::onMenuSignal(const Glib::ustring&, const Glib::ustring& signal_name,
                        const Glib::VariantContainerBase&) {
  if (signal_name == "LayoutUpdated") {
    menu_needs_rebuild_ = true;
    refreshMenuLayout();
  }
}

void Item::refreshMenuLayout() {
  if (!menu_proxy_) return;
  auto params = Glib::VariantContainerBase::create_tuple(
      {Glib::Variant<int>::create(0),   // parent id (root)
       Glib::Variant<int>::create(-1),  // recurse all levels
       Glib::Variant<std::vector<Glib::ustring>>::create(
           {"label", "type", "children-display", "visible"})});
  menu_proxy_->call("GetLayout", sigc::mem_fun(*this, &Item::onMenuLayoutReceived), params);
}

void Item::onMenuLayoutReceived(Glib::RefPtr<Gio::AsyncResult>& result) {
  try {
    auto value = menu_proxy_->call_finish(result);
    auto tuple = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(value);
    // GetLayout returns (u(ia{sv}av)); child 1 is the root layout item.
    auto root = tuple.get_child(1);

    menu_actions_ = Gio::SimpleActionGroup::create();
    widget_.insert_action_group("sni", menu_actions_);
    menu_model_ = Gio::Menu::create();
    parseMenuLayout(root, menu_model_);
    menu_needs_rebuild_ = false;

    if (menu_show_pending_) {
      menu_show_pending_ = false;
      showDbusMenu(menu_pending_x_, menu_pending_y_);
    }
  } catch (const Glib::Error& err) {
    spdlog::warn("Failed to parse DBusMenu layout for {}: {}", id, err.what());
  } catch (const std::exception& err) {
    spdlog::warn("Failed to parse DBusMenu layout for {}: {}", id, err.what());
  }
  menu_show_pending_ = false;
}

void Item::parseMenuLayout(const Glib::VariantBase& variant,
                           const Glib::RefPtr<Gio::Menu>& target) {
  Glib::VariantBase current = variant;
  // DBusMenu returns children in an 'av' where each element is a variant 'v'
  // wrapping the real '(ia{sv}av)' tuple. Unwrap it before casting.
  if (current.get_type_string() == "v") {
    Glib::VariantBase inner(g_variant_get_variant(current.gobj()), false);
    current = inner;
  }

  auto tuple = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(current);
  int id = Glib::VariantBase::cast_dynamic<Glib::Variant<int>>(tuple.get_child(0)).get();

  auto props_var =
      Glib::VariantBase::cast_dynamic<Glib::Variant<std::map<Glib::ustring, Glib::VariantBase>>>(
          tuple.get_child(1));
  auto props = props_var.get();

  auto children_var =
      Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<Glib::VariantBase>>>(
          tuple.get_child(2));
  auto children = children_var.get();

  if (id == 0) {
    for (const auto& child : children) {
      parseMenuLayout(child, target);
    }
    return;
  }

  std::string label;
  std::string type;
  std::string children_display;
  bool visible = true;

  if (auto it = props.find("label"); it != props.end()) {
    label = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(it->second).get();
  }
  if (auto it = props.find("type"); it != props.end()) {
    type = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(it->second).get();
  }
  if (auto it = props.find("children-display"); it != props.end()) {
    children_display =
        Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(it->second).get();
  }
  if (auto it = props.find("visible"); it != props.end()) {
    visible = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(it->second).get();
  }

  if (!visible) return;
  if (type == "separator") {
    return;
  }

  if (children_display == "submenu" && !children.empty()) {
    auto submenu = Gio::Menu::create();
    for (const auto& child : children) {
      parseMenuLayout(child, submenu);
    }
    target->append_submenu(label, submenu);
  } else {
    std::string action_name = "item-" + std::to_string(id);
    auto action = Gio::SimpleAction::create(action_name);
    action->signal_activate().connect([this, id](const Glib::VariantBase&) { sendMenuEvent(id); });
    menu_actions_->add_action(action);
    target->append(label, "sni." + action_name);
  }
}

void Item::sendMenuEvent(int id) {
  if (!menu_proxy_) return;
  // Event signature: Event(in i id, in s eventId, in v data, in u timestamp)
  auto inner = Glib::Variant<bool>::create(false);
  auto data = Glib::Variant<Glib::VariantBase>::create(inner);
  auto params = Glib::VariantContainerBase::create_tuple(
      {Glib::Variant<int>::create(id), Glib::Variant<Glib::ustring>::create("clicked"), data,
       Glib::Variant<uint32_t>::create(0)});
  menu_proxy_->call("Event", params);
}

void Item::showDbusMenu(double x, double y) {
  if (!menu_proxy_ || menu.empty()) return;

  if (menu_needs_rebuild_ || !menu_model_ || menu_model_->get_n_items() == 0) {
    if (!menu_show_pending_) {
      refreshMenuLayout();
    }
    menu_show_pending_ = true;
    menu_pending_x_ = x;
    menu_pending_y_ = y;
    return;
  }

  if (!popover_menu_) {
    popover_menu_ = Gtk::make_managed<Gtk::PopoverMenu>();
    popover_menu_->set_parent(widget_);
  }
  if (menu_model_) {
    popover_menu_->set_menu_model(menu_model_);
  }

  // Notify the remote side that we are about to show the root menu
  auto about_params = Glib::VariantContainerBase::create_tuple({Glib::Variant<int>::create(0)});
  menu_proxy_->call("AboutToShow", about_params);

  Gdk::Rectangle rect(static_cast<int>(x), static_cast<int>(y), 1, 1);
  popover_menu_->set_pointing_to(rect);
  popover_menu_->popup();
}

void Item::setProperty(const Glib::ustring& name, Glib::VariantBase& value) {
  try {
    spdlog::trace("Set tray item property: {}.{} = {}", id.empty() ? bus_name : id, name, value);

    if (name == "Category") {
      category = get_variant<std::string>(value);
    } else if (name == "Id") {
      id = get_variant<std::string>(value);

      /*
       * HACK: Electron apps seem to have the same ID, but tooltip seems correct, so use that as ID
       * to pass as the custom icon option. I'm avoiding being disruptive and setting that to the ID
       * itself as I've no idea what this would affect.
       * The tooltip text is converted to lowercase since that's what (most?) themes expect?
       * I still haven't found a way for it to pick from theme automatically, although
       * it might be my theme.
       */
      std::string icon_id = id;
      if (id == "chrome_status_icon_1") {
        Glib::VariantBase value;
        this->proxy_->get_cached_property(value, "ToolTip");
        tooltip = get_variant<ToolTip>(value);
        if (!tooltip.text.empty()) {
          icon_id = tooltip.text.lowercase();
          setCustomIcon(icon_id);
        }
      } else {
        setCustomIcon(icon_id);
      }

      // Check if this item should be hidden
      if (IconManager::instance().isHidden(icon_id)) {
        spdlog::debug("Hiding tray item with ID: {}", icon_id);
        is_hidden_ = true;
      }
    } else if (name == "Title") {
      title = get_variant<std::string>(value);
      if (tooltip.text.empty()) {
        widget_.set_tooltip_markup(title);
      }
    } else if (name == "Status") {
      setStatus(get_variant<Glib::ustring>(value));
    } else if (name == "IconName") {
      if (has_custom_icon_) {
        spdlog::trace("Item '{}': ignoring IconName update, custom icon is set", id);
      } else {
        icon_name = get_variant<std::string>(value);
      }
    } else if (name == "IconPixmap") {
      if (has_custom_icon_) {
        spdlog::trace("Item '{}': ignoring IconPixmap update, custom icon is set", id);
      } else {
        icon_pixmap = this->extractPixBuf(value.gobj());
      }
    } else if (name == "OverlayIconName") {
      overlay_icon_name = get_variant<std::string>(value);
    } else if (name == "OverlayIconPixmap") {
      overlay_icon_pixmap = extractPixBuf(value.gobj());
    } else if (name == "AttentionIconName") {
      attention_icon_name = get_variant<std::string>(value);
    } else if (name == "AttentionIconPixmap") {
      attention_icon_pixmap = extractPixBuf(value.gobj());
    } else if (name == "AttentionMovieName") {
      attention_movie_name = get_variant<std::string>(value);
    } else if (name == "ToolTip") {
      tooltip = get_variant<ToolTip>(value);
      if (!tooltip.text.empty()) {
        widget_.set_tooltip_markup(tooltip.text);
      }
    } else if (name == "IconThemePath") {
      icon_theme_path = get_variant<std::string>(value);
      if (!icon_theme_path.empty()) {
        icon_theme->set_search_path({icon_theme_path});
      }
    } else if (name == "Menu") {
      menu = get_variant<std::string>(value);
      if (!menu.empty()) {
        setupDbusMenu();
      }
    } else if (name == "ItemIsMenu") {
      item_is_menu = get_variant<bool>(value);
    }
  } catch (const Glib::Error& err) {
    spdlog::warn("Failed to set tray item property: {}.{}, value = {}, err = {}",
                 id.empty() ? bus_name : id, name, value, err.what());
  } catch (const std::exception& err) {
    spdlog::warn("Failed to set tray item property: {}.{}, value = {}, err = {}",
                 id.empty() ? bus_name : id, name, value, err.what());
  }
}

void Item::setStatus(const Glib::ustring& value) {
  status_ = value.lowercase();
  widget_.set_visible(!is_hidden_ && (show_passive_ || status_.compare("passive") != 0));

  auto classes = widget_.get_css_classes();
  for (const auto& class_name : classes) {
    widget_.remove_css_class(class_name);
  }
  auto css_class = status_;
  if (css_class.compare("needsattention") == 0) {
    // convert status to dash-case for CSS
    css_class = "needs-attention";
  }
  widget_.add_css_class(css_class);
  on_updated_();
}

void Item::setReady() {
  if (ready_) {
    return;
  }
  ready_ = true;
  on_ready_(*this);
}

void Item::invalidate() {
  if (ready_) {
    ready_ = false;
  }
  on_invalidate_(*this);
}

void Item::setCustomIcon(const std::string& id) {
  spdlog::debug("SNI tray id: {}", id);

  if (order_ == -1) {
    auto iter = orders_.find(id);
    if (iter != orders_.end()) {
      order_ = iter->second;
      spdlog::debug("reordering tray item {}, order: {}", id, order_);
    } else {
      order_ = 0;
    }
    host_.reorderItems();
  }

  std::string custom_icon = IconManager::instance().getIconForApp(id);
  if (!custom_icon.empty()) {
    if (std::filesystem::exists(custom_icon)) {
      try {
        Glib::RefPtr<Gdk::Pixbuf> custom_pixbuf = Gdk::Pixbuf::create_from_file(custom_icon);
        icon_name = "";  // icon_name has priority over pixmap
        icon_pixmap = custom_pixbuf;
        has_custom_icon_ = true;
      } catch (const Glib::Error& e) {
        spdlog::error("Failed to load custom icon {}: {}", custom_icon, e.what());
      }
    } else {  // if file doesn't exist it's most likely an icon_name
      icon_name = custom_icon;
      has_custom_icon_ = true;
    }
  }
}

void Item::getUpdatedProperties() {
  auto params = Glib::VariantContainerBase::create_tuple(
      {Glib::Variant<Glib::ustring>::create(SNI_INTERFACE_NAME)});
  proxy_->call("org.freedesktop.DBus.Properties.GetAll",
               sigc::mem_fun(*this, &Item::processUpdatedProperties), params);
};

void Item::processUpdatedProperties(Glib::RefPtr<Gio::AsyncResult>& _result) {
  try {
    auto result = proxy_->call_finish(_result);
    // extract "a{sv}" from VariantContainerBase
    Glib::Variant<std::map<Glib::ustring, Glib::VariantBase>> properties_variant;
    result.get_child(properties_variant);
    auto properties = properties_variant.get();

    for (const auto& [name, value] : properties) {
      if (update_pending_.count(name.raw())) {
        setProperty(name, const_cast<Glib::VariantBase&>(value));
      }
    }

    this->updateImage();
  } catch (const Glib::Error& err) {
    spdlog::warn("Failed to update properties: {}", err.what());
  } catch (const std::exception& err) {
    spdlog::warn("Failed to update properties: {}", err.what());
  }
  update_pending_.clear();
}

/**
 * Mapping from a signal name to a set of possibly changed properties.
 * Commented signals are not handled by the tray module at the moment.
 */
static const std::map<std::string_view, std::set<std::string_view>> signal2props = {
    {"NewTitle", {"Title"}},
    {"NewIcon", {"IconName", "IconPixmap"}},
    {"NewAttentionIcon", {"AttentionIconName", "AttentionIconPixmap", "AttentionMovieName"}},
    {"NewOverlayIcon", {"OverlayIconName", "OverlayIconPixmap"}},
    {"NewIconThemePath", {"IconThemePath"}},
    {"NewToolTip", {"ToolTip"}},
    {"NewStatus", {"Status"}},
    // {"XAyatanaNewLabel", {"XAyatanaLabel"}},
};

void Item::onSignal(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                    const Glib::VariantContainerBase& arguments) {
  spdlog::trace("Tray item '{}' got signal {}", id, signal_name);
  auto changed = signal2props.find(signal_name.raw());
  if (changed != signal2props.end()) {
    if (update_pending_.empty()) {
      /* Debounce signals and schedule update of all properties.
       * Based on behavior of Plasma dataengine for StatusNotifierItem.
       */
      Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &Item::getUpdatedProperties),
                                          UPDATE_DEBOUNCE_TIME);
    }
    update_pending_.insert(changed->second.begin(), changed->second.end());
  }
}

static void pixbuf_data_deleter(const guint8* data) { g_free((void*)data); }

Glib::RefPtr<Gdk::Pixbuf> Item::extractPixBuf(GVariant* variant) {
  GVariantIter* it;
  g_variant_get(variant, "a(iiay)", &it);
  if (it == nullptr) {
    return Glib::RefPtr<Gdk::Pixbuf>{};
  }
  GVariant* val;
  gint lwidth = 0;
  gint lheight = 0;
  gint width;
  gint height;
  guchar* array = nullptr;
  while (g_variant_iter_loop(it, "(ii@ay)", &width, &height, &val)) {
    if (width > 0 && height > 0 && val != nullptr && width * height > lwidth * lheight) {
      auto size = g_variant_get_size(val);
      /* Sanity check */
      if (size == 4U * width * height) {
        /* Find the largest image */
        gconstpointer data = g_variant_get_data(val);
        if (data != nullptr) {
          if (array != nullptr) {
            g_free(array);
          }
          // We must allocate our own array because the data from GVariant is read-only
          // and we need to modify it to convert ARGB to RGBA.
          array = static_cast<guchar*>(g_malloc(size));

          // Copy and convert ARGB to RGBA in one pass to avoid g_memdup2 overhead
          const guchar* src = static_cast<const guchar*>(data);
          for (gsize i = 0; i < size; i += 4) {
            guchar alpha = src[i];
            array[i] = src[i + 1];
            array[i + 1] = src[i + 2];
            array[i + 2] = src[i + 3];
            array[i + 3] = alpha;
          }

          lwidth = width;
          lheight = height;
        }
      }
    }
  }
  g_variant_iter_free(it);
  if (array != nullptr) {
    return Gdk::Pixbuf::create_from_data(array, Gdk::Colorspace::RGB, true, 8, lwidth, lheight,
                                         4 * lwidth, &pixbuf_data_deleter);
  }
  return Glib::RefPtr<Gdk::Pixbuf>{};
}

void Item::updateImage() {
  auto pixbuf = getIconPixbuf();
  if (!pixbuf) return;
  auto scaled_icon_size = getScaledIconSize();

  // If the loaded icon is not square, assume that the icon height should match the
  // requested icon size, but the width is allowed to be different. As such, if the
  // height of the image does not match the requested icon size, resize the icon such that
  // the aspect ratio is maintained, but the height matches the requested icon size.
  if (pixbuf->get_height() > 0 && pixbuf->get_height() != scaled_icon_size) {
    int width = scaled_icon_size * pixbuf->get_width() / pixbuf->get_height();
    pixbuf = pixbuf->scale_simple(width, scaled_icon_size, Gdk::InterpType::BILINEAR);
  }

  pixbuf = overlayPixbufs(pixbuf, getOverlayIconPixbuf());

  auto texture = Gdk::Texture::create_for_pixbuf(pixbuf);
  image.set(texture);
}

Glib::RefPtr<Gdk::Pixbuf> Item::getIconPixbuf() {
  if (status_ == "needsattention") {
    if (auto attention_pixbuf = getAttentionIconPixbuf()) {
      return attention_pixbuf;
    }
  }

  if (auto pixbuf = loadIconFromNameOrFile(icon_name, true)) {
    return pixbuf;
  }

  if (icon_pixmap) {
    return icon_pixmap;
  }

  if (icon_name.empty()) {
    spdlog::error("Item '{}': No icon name or pixmap given.", id);
  } else {
    spdlog::error("Item '{}': Could not find an icon named '{}' and no pixmap given.", id,
                  icon_name);
  }

  return getIconByName("image-missing", getScaledIconSize());
}

Glib::RefPtr<Gdk::Pixbuf> Item::getAttentionIconPixbuf() {
  if (auto pixbuf = loadIconFromNameOrFile(attention_icon_name, false)) {
    return pixbuf;
  }
  if (auto pixbuf = loadIconFromNameOrFile(attention_movie_name, false)) {
    return pixbuf;
  }
  return attention_icon_pixmap;
}

Glib::RefPtr<Gdk::Pixbuf> Item::getOverlayIconPixbuf() {
  if (auto pixbuf = loadIconFromNameOrFile(overlay_icon_name, false)) {
    return pixbuf;
  }
  return overlay_icon_pixmap;
}

Glib::RefPtr<Gdk::Pixbuf> Item::loadIconFromNameOrFile(const std::string& name, bool log_failure) {
  if (name.empty()) {
    return {};
  }

  try {
    std::ifstream temp(name);
    if (temp.is_open()) {
      return Gdk::Pixbuf::create_from_file(name);
    }
  } catch (const Glib::Error& e) {
    if (log_failure) {
      spdlog::warn("Item '{}': {}", id, static_cast<std::string>(e.what()));
    }
  }

  try {
    return getIconByName(name, getScaledIconSize());
  } catch (const Glib::Error& e) {
    if (log_failure) {
      spdlog::trace("Item '{}': {}", id, static_cast<std::string>(e.what()));
    }
  }

  return {};
}

Glib::RefPtr<Gdk::Pixbuf> Item::overlayPixbufs(const Glib::RefPtr<Gdk::Pixbuf>& base,
                                               const Glib::RefPtr<Gdk::Pixbuf>& overlay) {
  if (!base || !overlay) {
    return base;
  }

  auto composed = base->copy();
  if (!composed) {
    return base;
  }

  int overlay_target_size =
      std::max(1, std::min(composed->get_width(), composed->get_height()) / 2);
  auto scaled_overlay =
      overlay->scale_simple(overlay_target_size, overlay_target_size, Gdk::InterpType::BILINEAR);
  if (!scaled_overlay) {
    return composed;
  }

  int dest_x = std::max(0, composed->get_width() - scaled_overlay->get_width());
  int dest_y = std::max(0, composed->get_height() - scaled_overlay->get_height());
  scaled_overlay->composite(composed, dest_x, dest_y, scaled_overlay->get_width(),
                            scaled_overlay->get_height(), dest_x, dest_y, 1.0, 1.0,
                            Gdk::InterpType::BILINEAR, 255);
  return composed;
}

Glib::RefPtr<Gdk::Pixbuf> Item::getIconByName(const std::string& name, int request_size) {
  auto load_from_paintable =
      [](const Glib::RefPtr<Gtk::IconPaintable>& paintable) -> Glib::RefPtr<Gdk::Pixbuf> {
    if (!paintable) return {};
    auto file = paintable->get_file();
    if (file) {
      try {
        return Gdk::Pixbuf::create_from_file(file->get_path());
      } catch (...) {
        return {};
      }
    }
    return {};
  };

  if (!icon_theme_path.empty()) {
    auto paintable = icon_theme->lookup_icon(name, request_size, widget_.get_scale_factor(),
                                             Gtk::TextDirection::NONE, Gtk::IconLookupFlags{});
    if (auto pb = load_from_paintable(paintable)) return pb;
  }

  auto default_theme = Gtk::IconTheme::get_for_display(widget_.get_display());
  if (default_theme) {
    auto paintable = default_theme->lookup_icon(name, request_size, widget_.get_scale_factor(),
                                                Gtk::TextDirection::NONE, Gtk::IconLookupFlags{});
    if (auto pb = load_from_paintable(paintable)) return pb;
  }
  return {};
}

double Item::getScaledIconSize() {
  // apply the scale factor from the Gtk window to the requested icon size
  return icon_size * widget_.get_scale_factor();
}

void Item::handleClick(int n_press, double x, double y) {
  if (!proxy_) {
    return;
  }
  // Claim this sequence so the event does not bubble up to the Bar or Tray
  gesture_click_->set_state(Gtk::EventSequenceState::CLAIMED);

  int button = gesture_click_->get_current_button();
  auto parameters = Glib::VariantContainerBase::create_tuple(
      {Glib::Variant<int>::create(static_cast<int>(x) + bar_.x_global),
       Glib::Variant<int>::create(static_cast<int>(y) + bar_.y_global)});

  if ((button == 1 && item_is_menu) || button == 3) {
    if (!menu.empty() && menu_proxy_ && menu_model_ && menu_model_->get_n_items() > 0) {
      showDbusMenu(x, y);
    } else {
      proxy_->call("ContextMenu", parameters);
    }
  } else if (button == 1) {
    proxy_->call("Activate", parameters);
  } else if (button == 2) {
    proxy_->call("SecondaryActivate", parameters);
  }
}

bool Item::handleScroll(double dx, double dy) {
  if (!proxy_) {
    return false;
  }

  distance_scrolled_x_ += dx;
  distance_scrolled_y_ += dy;

  int idelta_x = 0, idelta_y = 0;

  if (distance_scrolled_x_ > scroll_threshold_) {
    idelta_x = (int)lround(std::max(distance_scrolled_x_, 1.0));
    distance_scrolled_x_ = 0;
  } else if (distance_scrolled_x_ < -scroll_threshold_) {
    idelta_x = (int)lround(std::min(distance_scrolled_x_, -1.0));
    distance_scrolled_x_ = 0;
  }

  if (distance_scrolled_y_ > scroll_threshold_) {
    idelta_y = (int)lround(std::max(distance_scrolled_y_, 1.0));
    distance_scrolled_y_ = 0;
  } else if (distance_scrolled_y_ < -scroll_threshold_) {
    idelta_y = (int)lround(std::min(distance_scrolled_y_, -1.0));
    distance_scrolled_y_ = 0;
  }

  if (idelta_x != 0) {
    auto parameters = Glib::VariantContainerBase::create_tuple(
        {Glib::Variant<int>::create(idelta_x), Glib::Variant<Glib::ustring>::create("horizontal")});
    proxy_->call("Scroll", parameters);
  }
  if (idelta_y != 0) {
    auto parameters = Glib::VariantContainerBase::create_tuple(
        {Glib::Variant<int>::create(idelta_y), Glib::Variant<Glib::ustring>::create("vertical")});
    proxy_->call("Scroll", parameters);
  }
  return true;
}

}  // namespace waybar::modules::SNI
