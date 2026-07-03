#include "modules/sni/item.hpp"

#include <gdkmm/general.h>
#include <glibmm/main.h>
#include <gtkmm/tooltip.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>

#include "gdk/gdk.h"
#include "modules/sni/icon_manager.hpp"
#include "util/format.hpp"
#include "util/gtk_icon.hpp"

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
           const std::function<void(Item&)>& on_invalidate, const std::function<void()>& on_updated)
    : bus_name(bn),
      object_path(op),
      icon_size(16),
      effective_icon_size(0),
      icon_theme(Gtk::IconTheme::create()),
      bar_(bar),
      on_ready_(on_ready),
      on_invalidate_(on_invalidate),
      on_updated_(on_updated) {
  if (config["icon-size"].isUInt()) {
    icon_size = config["icon-size"].asUInt();
  }
  if (config["smooth-scrolling-threshold"].isNumeric()) {
    scroll_threshold_ = config["smooth-scrolling-threshold"].asDouble();
  }
  if (config["show-passive-items"].isBool()) {
    show_passive_ = config["show-passive-items"].asBool();
  }

  auto& window = const_cast<Bar&>(bar).window;
  window.signal_configure_event().connect_notify(sigc::mem_fun(*this, &Item::onConfigure));
  event_box.add(image);
  event_box.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  event_box.signal_button_press_event().connect(sigc::mem_fun(*this, &Item::handleClick));
  event_box.signal_scroll_event().connect(sigc::mem_fun(*this, &Item::handleScroll));
  event_box.signal_enter_notify_event().connect(sigc::mem_fun(*this, &Item::handleMouseEnter));
  event_box.signal_leave_notify_event().connect(sigc::mem_fun(*this, &Item::handleMouseLeave));
  // initial visibility
  event_box.show_all();
  event_box.set_visible(show_passive_);

  cancellable_ = Gio::Cancellable::create();

  auto interface = Glib::wrap(sn_item_interface_info(), true);
  Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::BUS_TYPE_SESSION, bus_name, object_path,
                                   SNI_INTERFACE_NAME, sigc::mem_fun(*this, &Item::proxyReady),
                                   cancellable_, interface);
}

Item::~Item() {
  if (this->gtk_menu != nullptr) {
    this->gtk_menu->popdown();
    this->gtk_menu->detach();
  }
  if (this->dbus_menu != nullptr) {
    g_object_weak_unref(G_OBJECT(this->dbus_menu), (GWeakNotify)onMenuDestroyed, this);
    this->dbus_menu = nullptr;
  }
}

bool Item::isReady() const { return ready_; }

bool Item::handleMouseEnter(GdkEventCrossing* const& e) {
  event_box.set_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
  return false;
}

bool Item::handleMouseLeave(GdkEventCrossing* const& e) {
  event_box.unset_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
  return false;
}

void Item::onConfigure(GdkEventConfigure* ev) { this->updateImage(); }

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
      if (id == "chrome_status_icon_1") {
        Glib::VariantBase value;
        this->proxy_->get_cached_property(value, "ToolTip");
        tooltip = get_variant<ToolTip>(value);
        if (!tooltip.text.empty()) {
          setCustomIcon(tooltip.text.lowercase());
        }
      } else {
        setCustomIcon(id);
      }
    } else if (name == "Title") {
      title = get_variant<std::string>(value);
      if (tooltip.text.empty()) {
        event_box.set_tooltip_markup(title);
      }
    } else if (name == "Status") {
      setStatus(get_variant<Glib::ustring>(value));
    } else if (name == "IconName") {
      icon_name = get_variant<std::string>(value);
    } else if (name == "IconPixmap") {
      icon_pixmap = this->extractPixBuf(value.gobj());
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
        event_box.set_tooltip_markup(tooltip.text);
      }
    } else if (name == "IconThemePath") {
      icon_theme_path = get_variant<std::string>(value);
      if (!icon_theme_path.empty()) {
        icon_theme->set_search_path({icon_theme_path});
      }
    } else if (name == "Menu") {
      menu = get_variant<std::string>(value);
      makeMenu();
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
  event_box.set_visible(show_passive_ || status_.compare("passive") != 0);

  auto style = event_box.get_style_context();
  for (const auto& class_name : style->list_classes()) {
    style->remove_class(class_name);
  }
  auto css_class = status_;
  if (css_class.compare("needsattention") == 0) {
    // convert status to dash-case for CSS
    css_class = "needs-attention";
  }
  style->add_class(css_class);
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

  std::string custom_icon = IconManager::instance().getIconForApp(id);
  if (!custom_icon.empty()) {
    if (std::filesystem::exists(custom_icon)) {
      try {
        Glib::RefPtr<Gdk::Pixbuf> custom_pixbuf = Gdk::Pixbuf::create_from_file(custom_icon);
        icon_name = "";  // icon_name has priority over pixmap
        icon_pixmap = custom_pixbuf;
      } catch (const Glib::Error& e) {
        spdlog::error("Failed to load custom icon {}: {}", custom_icon, e.what());
      }
    } else {  // if file doesn't exist it's most likely an icon_name
      icon_name = custom_icon;
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
    return Gdk::Pixbuf::create_from_data(array, Gdk::Colorspace::COLORSPACE_RGB, true, 8, lwidth,
                                         lheight, 4 * lwidth, &pixbuf_data_deleter);
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
    pixbuf = pixbuf->scale_simple(width, scaled_icon_size, Gdk::InterpType::INTERP_BILINEAR);
  }

  pixbuf = overlayPixbufs(pixbuf, getOverlayIconPixbuf());

  auto surface =
      Gdk::Cairo::create_surface_from_pixbuf(pixbuf, image.get_scale_factor(), image.get_window());
  image.set(surface);
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
  auto scaled_overlay = overlay->scale_simple(overlay_target_size, overlay_target_size,
                                              Gdk::InterpType::INTERP_BILINEAR);
  if (!scaled_overlay) {
    return composed;
  }

  int dest_x = std::max(0, composed->get_width() - scaled_overlay->get_width());
  int dest_y = std::max(0, composed->get_height() - scaled_overlay->get_height());
  scaled_overlay->composite(composed, dest_x, dest_y, scaled_overlay->get_width(),
                            scaled_overlay->get_height(), dest_x, dest_y, 1.0, 1.0,
                            Gdk::InterpType::INTERP_BILINEAR, 255);
  return composed;
}

Glib::RefPtr<Gdk::Pixbuf> Item::getIconByName(const std::string& name, int request_size) {
  if (!icon_theme_path.empty()) {
    auto icon_info = icon_theme->lookup_icon(name.c_str(), request_size,
                                             Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE);
    if (icon_info) {
      bool is_sym = false;
      return icon_info.load_symbolic(event_box.get_style_context(), is_sym);
    }
  }
  return DefaultGtkIconThemeWrapper::load_icon(name.c_str(), request_size,
                                               Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE,
                                               event_box.get_style_context());
}

double Item::getScaledIconSize() {
  // apply the scale factor from the Gtk window to the requested icon size
  return icon_size * image.get_scale_factor();
}

void Item::onMenuDestroyed(Item* self, GObject* old_menu_pointer) {
  if (old_menu_pointer == reinterpret_cast<GObject*>(self->dbus_menu)) {
    self->gtk_menu = nullptr;
    self->dbus_menu = nullptr;
  }
}

void Item::makeMenu() {
  if (gtk_menu == nullptr && !menu.empty()) {
    dbus_menu = dbusmenu_gtkmenu_new(bus_name.data(), menu.data());
    if (dbus_menu != nullptr) {
      g_object_ref_sink(G_OBJECT(dbus_menu));
      g_object_weak_ref(G_OBJECT(dbus_menu), (GWeakNotify)onMenuDestroyed, this);
      gtk_menu = Glib::wrap(GTK_MENU(dbus_menu));
      gtk_menu->attach_to_widget(event_box);
    }
  }
  // Manually reset prelight to make sure the tray item doesn't stay in a hover state even though
  // the menu is focused
  event_box.unset_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
}

bool Item::handleClick(GdkEventButton* const& ev) {
  if (!proxy_) {
    return false;
  }
  auto parameters = Glib::VariantContainerBase::create_tuple(
      {Glib::Variant<int>::create(ev->x_root + bar_.x_global),
       Glib::Variant<int>::create(ev->y_root + bar_.y_global)});
  if ((ev->button == 1 && item_is_menu) || ev->button == 3) {
    makeMenu();
    if (gtk_menu != nullptr) {
#if GTK_CHECK_VERSION(3, 22, 0)
      gtk_menu->popup_at_pointer(reinterpret_cast<GdkEvent*>(ev));
#else
      gtk_menu->popup(ev->button, ev->time);
#endif
      return true;
    } else {
      proxy_->call("ContextMenu", parameters);
      return true;
    }
  } else if (ev->button == 1) {
    proxy_->call("Activate", parameters);
    return true;
  } else if (ev->button == 2) {
    proxy_->call("SecondaryActivate", parameters);
    return true;
  }
  return false;
}

bool Item::handleScroll(GdkEventScroll* const& ev) {
  if (!proxy_) {
    return false;
  }
  int dx = 0, dy = 0;
  switch (ev->direction) {
    case GDK_SCROLL_UP:
      dy = -1;
      break;
    case GDK_SCROLL_DOWN:
      dy = 1;
      break;
    case GDK_SCROLL_LEFT:
      dx = -1;
      break;
    case GDK_SCROLL_RIGHT:
      dx = 1;
      break;
    case GDK_SCROLL_SMOOTH:
      distance_scrolled_x_ += ev->delta_x;
      distance_scrolled_y_ += ev->delta_y;
      // check against the configured threshold and ensure that the absolute value >= 1
      if (distance_scrolled_x_ > scroll_threshold_) {
        dx = (int)lround(std::max(distance_scrolled_x_, 1.0));
        distance_scrolled_x_ = 0;
      } else if (distance_scrolled_x_ < -scroll_threshold_) {
        dx = (int)lround(std::min(distance_scrolled_x_, -1.0));
        distance_scrolled_x_ = 0;
      }
      if (distance_scrolled_y_ > scroll_threshold_) {
        dy = (int)lround(std::max(distance_scrolled_y_, 1.0));
        distance_scrolled_y_ = 0;
      } else if (distance_scrolled_y_ < -scroll_threshold_) {
        dy = (int)lround(std::min(distance_scrolled_y_, -1.0));
        distance_scrolled_y_ = 0;
      }
      break;
  }
  if (dx != 0) {
    auto parameters = Glib::VariantContainerBase::create_tuple(
        {Glib::Variant<int>::create(dx), Glib::Variant<Glib::ustring>::create("horizontal")});
    proxy_->call("Scroll", parameters);
  }
  if (dy != 0) {
    auto parameters = Glib::VariantContainerBase::create_tuple(
        {Glib::Variant<int>::create(dy), Glib::Variant<Glib::ustring>::create("vertical")});
    proxy_->call("Scroll", parameters);
  }
  return true;
}

}  // namespace waybar::modules::SNI
