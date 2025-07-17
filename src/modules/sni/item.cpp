#include "modules/sni/item.hpp"

#include <gdkmm/general.h>
#include <glibmm/main.h>
#include <gtkmm/tooltip.h>
#include <spdlog/spdlog.h>

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

Item::Item(const std::string& bn, const std::string& op, const Json::Value& config, const Bar& bar)
    : bus_name(bn),
      object_path(op),
      icon_size(16),
      effective_icon_size(0),
      icon_theme(Gtk::IconTheme::create()),
      bar_(bar) {
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
      return;
    }
    this->updateImage();

  } catch (const Glib::Error& err) {
    spdlog::error("Failed to create DBus Proxy for {} {}: {}", bus_name, object_path, err.what());
  } catch (const std::exception& err) {
    spdlog::error("Failed to create DBus Proxy for {} {}: {}", bus_name, object_path, err.what());
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
      setCustomIcon(id);
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
      // TODO: overlay_icon_pixmap
    } else if (name == "AttentionIconName") {
      attention_icon_name = get_variant<std::string>(value);
    } else if (name == "AttentionIconPixmap") {
      // TODO: attention_icon_pixmap
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
  Glib::ustring lower = value.lowercase();
  event_box.set_visible(show_passive_ || lower.compare("passive") != 0);

  auto style = event_box.get_style_context();
  for (const auto& class_name : style->list_classes()) {
    style->remove_class(class_name);
  }
  if (lower.compare("needsattention") == 0) {
    // convert status to dash-case for CSS
    lower = "needs-attention";
  }
  style->add_class(lower);
}

void Item::setCustomIcon(const std::string& id) {
  std::string custom_icon = IconManager::instance().getIconForApp(id);
  if (!custom_icon.empty()) {
    if (std::filesystem::exists(custom_icon)) {
      Glib::RefPtr<Gdk::Pixbuf> custom_pixbuf = Gdk::Pixbuf::create_from_file(custom_icon);
      icon_name = "";  // icon_name has priority over pixmap
      icon_pixmap = custom_pixbuf;
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
    // {"NewAttentionIcon", {"AttentionIconName", "AttentionIconPixmap", "AttentionMovieName"}},
    // {"NewOverlayIcon", {"OverlayIconName", "OverlayIconPixmap"}},
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
#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 68
          array = static_cast<guchar*>(g_memdup2(data, size));
#else
          array = static_cast<guchar*>(g_memdup(data, size));
#endif
          lwidth = width;
          lheight = height;
        }
      }
    }
  }
  g_variant_iter_free(it);
  if (array != nullptr) {
    /* argb to rgba */
    for (uint32_t i = 0; i < 4U * lwidth * lheight; i += 4) {
      guchar alpha = array[i];
      array[i] = array[i + 1];
      array[i + 1] = array[i + 2];
      array[i + 2] = array[i + 3];
      array[i + 3] = alpha;
    }
    return Gdk::Pixbuf::create_from_data(array, Gdk::Colorspace::COLORSPACE_RGB, true, 8, lwidth,
                                         lheight, 4 * lwidth, &pixbuf_data_deleter);
  }
  return Glib::RefPtr<Gdk::Pixbuf>{};
}

void Item::updateImage() {
  auto pixbuf = getIconPixbuf();
  auto scaled_icon_size = getScaledIconSize();

  // If the loaded icon is not square, assume that the icon height should match the
  // requested icon size, but the width is allowed to be different. As such, if the
  // height of the image does not match the requested icon size, resize the icon such that
  // the aspect ratio is maintained, but the height matches the requested icon size.
  if (pixbuf->get_height() != scaled_icon_size) {
    int width = scaled_icon_size * pixbuf->get_width() / pixbuf->get_height();
    pixbuf = pixbuf->scale_simple(width, scaled_icon_size, Gdk::InterpType::INTERP_BILINEAR);
  }

  auto surface =
      Gdk::Cairo::create_surface_from_pixbuf(pixbuf, image.get_scale_factor(), image.get_window());
  image.set(surface);
}

Glib::RefPtr<Gdk::Pixbuf> Item::getIconPixbuf() {
  if (!icon_name.empty()) {
    try {
      std::ifstream temp(icon_name);
      if (temp.is_open()) {
        return Gdk::Pixbuf::create_from_file(icon_name);
      }
    } catch (Glib::Error& e) {
      // Ignore because we want to also try different methods of getting an icon.
      //
      // But a warning is logged, as the file apparently exists, but there was
      // a failure in creating a pixbuf out of it.

      spdlog::warn("Item '{}': {}", id, static_cast<std::string>(e.what()));
    }

    try {
      // Will throw if it can not find an icon.
      return getIconByName(icon_name, getScaledIconSize());
    } catch (Glib::Error& e) {
      spdlog::trace("Item '{}': {}", id, static_cast<std::string>(e.what()));
    }
  }

  // Return the pixmap only if an icon for the given name could not be found.
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

Glib::RefPtr<Gdk::Pixbuf> Item::getIconByName(const std::string& name, int request_size) {
  icon_theme->rescan_if_needed();

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
}

bool Item::handleClick(GdkEventButton* const& ev) {
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
