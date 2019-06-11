#include "modules/sni/item.hpp"
#include <glibmm/main.h>
#include <spdlog/spdlog.h>

template <>
struct fmt::formatter<Glib::ustring> : formatter<std::string> {
  template <typename FormatContext>
  auto format(const Glib::ustring& value, FormatContext& ctx) {
    return formatter<std::string>::format(value, ctx);
  }
};

template <>
struct fmt::formatter<Glib::VariantBase> : formatter<std::string> {
  bool is_printable(const Glib::VariantBase& value) {
    auto type = value.get_type_string();
    /* Print only primitive (single character excluding 'v') and short complex types */
    return (type.length() == 1 && islower(type[0]) && type[0] != 'v') || value.get_size() <= 32;
  }

  template <typename FormatContext>
  auto format(const Glib::VariantBase& value, FormatContext& ctx) {
    if (is_printable(value)) {
      return formatter<std::string>::format(value.print(), ctx);
    } else {
      return formatter<std::string>::format(value.get_type_string(), ctx);
    }
  }
};

namespace waybar::modules::SNI {

static const Glib::ustring SNI_INTERFACE_NAME = sn_item_interface_info()->name;
static const unsigned      UPDATE_DEBOUNCE_TIME = 10;

Item::Item(const std::string& bn, const std::string& op, const Json::Value& config)
    : bus_name(bn),
      object_path(op),
      icon_size(16),
      effective_icon_size(0),
      icon_theme(Gtk::IconTheme::create()),
      update_pending_(false) {
  if (config["icon-size"].isUInt()) {
    icon_size = config["icon-size"].asUInt();
  }
  event_box.add(image);
  event_box.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box.signal_button_press_event().connect(sigc::mem_fun(*this, &Item::handleClick));

  cancellable_ = Gio::Cancellable::create();

  auto interface = Glib::wrap(sn_item_interface_info(), true);
  Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::BUS_TYPE_SESSION,
                                   bus_name,
                                   object_path,
                                   SNI_INTERFACE_NAME,
                                   sigc::mem_fun(*this, &Item::proxyReady),
                                   cancellable_,
                                   interface);
}

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

    if (this->id.empty() || this->category.empty() || this->status.empty()) {
      spdlog::error("Invalid Status Notifier Item: {}, {}", bus_name, object_path);
      return;
    }
    this->updateImage();
    // this->event_box.set_tooltip_text(this->title);

  } catch (const Glib::Error& err) {
    spdlog::error("Failed to create DBus Proxy for {} {}: {}", bus_name, object_path, err.what());
  } catch (const std::exception& err) {
    spdlog::error("Failed to create DBus Proxy for {} {}: {}", bus_name, object_path, err.what());
  }
}

template <typename T>
T get_variant(Glib::VariantBase& value) {
  return Glib::VariantBase::cast_dynamic<Glib::Variant<T>>(value).get();
}

void Item::setProperty(const Glib::ustring& name, Glib::VariantBase& value) {
  try {
    spdlog::trace("Set tray item property: {}.{} = {}", id.empty() ? bus_name : id, name, value);

    if (name == "Category") {
      category = get_variant<std::string>(value);
    } else if (name == "Id") {
      id = get_variant<std::string>(value);
    } else if (name == "Title") {
      title = get_variant<std::string>(value);
    } else if (name == "Status") {
      status = get_variant<std::string>(value);
    } else if (name == "WindowId") {
      window_id = get_variant<int32_t>(value);
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
      // TODO: tooltip
    } else if (name == "IconThemePath") {
      icon_theme_path = get_variant<std::string>(value);
      if (!icon_theme_path.empty()) {
        icon_theme->set_search_path({icon_theme_path});
      }
    } else if (name == "Menu") {
      menu = get_variant<std::string>(value);
    } else if (name == "ItemIsMenu") {
      item_is_menu = get_variant<bool>(value);
    }
  } catch (const Glib::Error& err) {
    spdlog::warn("Failed to set tray item property: {}.{}, value = {}, err = {}",
                 id.empty() ? bus_name : id,
                 name,
                 value,
                 err.what());
  } catch (const std::exception& err) {
    spdlog::warn("Failed to set tray item property: {}.{}, value = {}, err = {}",
                 id.empty() ? bus_name : id,
                 name,
                 value,
                 err.what());
  }
}

void Item::getUpdatedProperties() {
  update_pending_ = false;

  auto params = Glib::VariantContainerBase::create_tuple(
      {Glib::Variant<Glib::ustring>::create(SNI_INTERFACE_NAME)});
  proxy_->call("org.freedesktop.DBus.Properties.GetAll",
               sigc::mem_fun(*this, &Item::processUpdatedProperties),
               params);
};

void Item::processUpdatedProperties(Glib::RefPtr<Gio::AsyncResult>& _result) {
  try {
    auto result = proxy_->call_finish(_result);
    // extract "a{sv}" from VariantContainerBase
    Glib::Variant<std::map<Glib::ustring, Glib::VariantBase>> properties_variant;
    result.get_child(properties_variant);
    auto properties = properties_variant.get();

    for (const auto& [name, value] : properties) {
      Glib::VariantBase old_value;
      proxy_->get_cached_property(old_value, name);
      if (!old_value || !value.equal(old_value)) {
        proxy_->set_cached_property(name, value);
        setProperty(name, const_cast<Glib::VariantBase&>(value));
      }
    }

    this->updateImage();
    // this->event_box.set_tooltip_text(this->title);
  } catch (const Glib::Error& err) {
    spdlog::warn("Failed to update properties: {}", err.what());
  } catch (const std::exception& err) {
    spdlog::warn("Failed to update properties: {}", err.what());
  }
}

void Item::onSignal(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                    const Glib::VariantContainerBase& arguments) {
  spdlog::trace("Tray item '{}' got signal {}", id, signal_name);
  if (!update_pending_ && signal_name.compare(0, 3, "New") == 0) {
    /* Debounce signals and schedule update of all properties.
     * Based on behavior of Plasma dataengine for StatusNotifierItem.
     */
    update_pending_ = true;
    Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &Item::getUpdatedProperties),
                                        UPDATE_DEBOUNCE_TIME);
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
  gint      lwidth = 0;
  gint      lheight = 0;
  gint      width;
  gint      height;
  guchar*   array = nullptr;
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
          array = static_cast<guchar*>(g_memdup(data, size));
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
    return Gdk::Pixbuf::create_from_data(array,
                                         Gdk::Colorspace::COLORSPACE_RGB,
                                         true,
                                         8,
                                         lwidth,
                                         lheight,
                                         4 * lwidth,
                                         &pixbuf_data_deleter);
  }
  return Glib::RefPtr<Gdk::Pixbuf>{};
}

void Item::updateImage() {
  image.set_from_icon_name("image-missing", Gtk::ICON_SIZE_MENU);
  image.set_pixel_size(icon_size);
  if (!icon_name.empty()) {
    try {
      // Try to find icons specified by path and filename
#ifdef FILESYSTEM_EXPERIMENTAL
      if (std::experimental::filesystem::exists(icon_name)) {
#else
      if (std::filesystem::exists(icon_name)) {
#endif
        auto pixbuf = Gdk::Pixbuf::create_from_file(icon_name);
        if (pixbuf->gobj() != nullptr) {
          // An icon specified by path and filename may be the wrong size for
          // the tray
          pixbuf = pixbuf->scale_simple(icon_size, icon_size, Gdk::InterpType::INTERP_BILINEAR);
          image.set(pixbuf);
        }
      } else {
        image.set(getIconByName(icon_name, icon_size));
      }
    } catch (Glib::Error& e) {
      spdlog::error("Item '{}': {}", id, static_cast<std::string>(e.what()));
    }
  } else if (icon_pixmap) {
    // An icon extracted may be the wrong size for the tray
    icon_pixmap = icon_pixmap->scale_simple(icon_size, icon_size, Gdk::InterpType::INTERP_BILINEAR);
    image.set(icon_pixmap);
  }
}

Glib::RefPtr<Gdk::Pixbuf> Item::getIconByName(const std::string& name, int request_size) {
  int tmp_size = 0;
  icon_theme->rescan_if_needed();
  auto sizes = icon_theme->get_icon_sizes(name.c_str());

  for (auto const& size : sizes) {
    // -1 == scalable
    if (size == request_size || size == -1) {
      tmp_size = request_size;
      break;
    } else if (size < request_size) {
      tmp_size = size;
    } else if (size > tmp_size && tmp_size > 0) {
      tmp_size = request_size;
      break;
    }
  }
  if (tmp_size == 0) {
    tmp_size = request_size;
  }
  if (!icon_theme_path.empty() &&
      icon_theme->lookup_icon(
          name.c_str(), tmp_size, Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE)) {
    return icon_theme->load_icon(
        name.c_str(), tmp_size, Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE);
  }
  Glib::RefPtr<Gtk::IconTheme> default_theme = Gtk::IconTheme::get_default();
  default_theme->rescan_if_needed();
  return default_theme->load_icon(
      name.c_str(), tmp_size, Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE);
}

void Item::onMenuDestroyed(Item* self, GObject* old_menu_pointer) {
  if (old_menu_pointer == reinterpret_cast<GObject*>(self->dbus_menu)) {
    self->gtk_menu = nullptr;
    self->dbus_menu = nullptr;
  }
}

void Item::makeMenu(GdkEventButton* const& ev) {
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
      {Glib::Variant<int>::create(ev->x), Glib::Variant<int>::create(ev->y)});
  if ((ev->button == 1 && (item_is_menu || !menu.empty())) || ev->button == 3) {
    makeMenu(ev);
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

}  // namespace waybar::modules::SNI
