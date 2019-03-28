#include "modules/sni/item.hpp"

#include <iostream>

using namespace Glib;

static const ustring SNI_INTERFACE_NAME = sn_item_interface_info()->name;

waybar::modules::SNI::Item::Item(std::string bn, std::string op, const Json::Value& config)
    : bus_name(bn), object_path(op), icon_size(16), effective_icon_size(0)
{
  if (config["icon-size"].isUInt()) {
    icon_size = config["icon-size"].asUInt();
  }
  event_box.add(image);
  event_box.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box.signal_button_press_event().connect(
      sigc::mem_fun(*this, &Item::handleClick));

  cancellable_ = Gio::Cancellable::create();

  auto interface = Glib::wrap(sn_item_interface_info(), true);
  Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::BUS_TYPE_SESSION, bus_name,
      object_path, SNI_INTERFACE_NAME, sigc::mem_fun(*this, &Item::proxyReady),
      cancellable_, interface);
}

void waybar::modules::SNI::Item::proxyReady(Glib::RefPtr<Gio::AsyncResult>& result) {
  try {
    this->proxy_ = Gio::DBus::Proxy::create_for_bus_finish(result);
    /* Properties are already cached during object creation */
    auto cached_properties = this->proxy_->get_cached_property_names();
    for (const auto& name: cached_properties) {
      Glib::VariantBase value;
      this->proxy_->get_cached_property(value, name);
      setProperty(name, value);
    }

    if (this->id.empty() || this->category.empty() || this->status.empty()) {
      std::cerr << "Invalid Status Notifier Item: " + this->bus_name + "," +
        this->object_path << std::endl;
      return;
    }
    if (!this->icon_theme_path.empty()) {
      Glib::RefPtr<Gtk::IconTheme> icon_theme = Gtk::IconTheme::get_default();
      icon_theme->append_search_path(this->icon_theme_path);
    }
    this->updateImage();
    // this->event_box.set_tooltip_text(this->title);

  } catch (const Glib::Error& err) {
    g_error("Failed to create DBus Proxy for %s %s: %s", bus_name.c_str(),
        object_path.c_str(), err.what().c_str());
  } catch (const std::exception& err) {
    g_error("Failed to create DBus Proxy for %s %s: %s", bus_name.c_str(),
        object_path.c_str(), err.what());
  }
}

template<typename T>
T get_variant(VariantBase& value) {
    return VariantBase::cast_dynamic<Variant<T>>(value).get();
}

void
waybar::modules::SNI::Item::setProperty(const ustring& name,
    VariantBase& value) {
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
  } else if (name == "Menu") {
    menu = get_variant<std::string>(value);
  } else if (name == "ItemIsMenu") {
    item_is_menu = get_variant<bool>(value);
  }
}

static void
pixbuf_data_deleter(const guint8* data) {
  g_free((void*) data);
}

Glib::RefPtr<Gdk::Pixbuf>
waybar::modules::SNI::Item::extractPixBuf(GVariant *variant) {
  GVariantIter *it;
  g_variant_get(variant, "a(iiay)", &it);
  if (it == nullptr) {
    return Glib::RefPtr<Gdk::Pixbuf>{};
  }
  GVariant *val;
  gint lwidth = 0;
  gint lheight = 0;
  gint width;
  gint height;
  guchar *array = nullptr;
  while (g_variant_iter_loop(it, "(ii@ay)", &width, &height, &val)) {
    if (width > 0 && height > 0 && val != nullptr &&
        width * height > lwidth * lheight) {
      auto size = g_variant_get_size(val);
      /* Sanity check */
      if (size == 4U * width * height) {
        /* Find the largest image */
        gconstpointer data = g_variant_get_data(val);
        if (data != nullptr) {
          if (array != nullptr) {
            g_free(array);
          }
          array = static_cast<guchar *>(g_memdup(data, size));
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
    return Gdk::Pixbuf::create_from_data(array, Gdk::Colorspace::COLORSPACE_RGB,
                                         true, 8, lwidth, lheight, 4 * lwidth,
                                         &pixbuf_data_deleter);
  }
  return Glib::RefPtr<Gdk::Pixbuf>{};
}

void waybar::modules::SNI::Item::updateImage()
{
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
          pixbuf = pixbuf->scale_simple(icon_size, icon_size,
            Gdk::InterpType::INTERP_BILINEAR);
          image.set(pixbuf);
        }
      } else {
        image.set(getIconByName(icon_name, icon_size));
      }
    } catch (Glib::Error &e) {
      std::cerr << "Exception: " << e.what() << std::endl;
    }
  } else if (icon_pixmap) {
    // An icon extracted may be the wrong size for the tray
    icon_pixmap = icon_pixmap->scale_simple(icon_size, icon_size,
      Gdk::InterpType::INTERP_BILINEAR);
    image.set(icon_pixmap);
  }
}

Glib::RefPtr<Gdk::Pixbuf>
waybar::modules::SNI::Item::getIconByName(std::string name, int request_size) {
  int tmp_size = 0;
  Glib::RefPtr<Gtk::IconTheme> icon_theme = Gtk::IconTheme::get_default();
  icon_theme->rescan_if_needed();
  auto sizes = icon_theme->get_icon_sizes(name.c_str());

  for (auto const &size : sizes) {
    // -1 == scalable
    if (size == request_size || size == -1) {
      tmp_size = request_size;
      break;
    } else if (size < request_size || (size > tmp_size && tmp_size > 0)) {
      tmp_size = size;
    }
  }
  if (tmp_size == 0) {
    tmp_size = request_size;
  }
  return icon_theme->load_icon(name.c_str(), tmp_size,
    Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE);
}

void waybar::modules::SNI::Item::onMenuDestroyed(Item *self)
{
  self->gtk_menu = nullptr;
  self->dbus_menu = nullptr;
}

bool waybar::modules::SNI::Item::makeMenu(GdkEventButton *const &ev)
{
  if (gtk_menu == nullptr) {
    if (!menu.empty()) {
      dbus_menu = dbusmenu_gtkmenu_new(bus_name.data(), menu.data());
      if (dbus_menu != nullptr) {
        g_object_ref_sink(G_OBJECT(dbus_menu));
        g_object_weak_ref(G_OBJECT(dbus_menu), (GWeakNotify)onMenuDestroyed, this);
        gtk_menu = Glib::wrap(GTK_MENU(dbus_menu));
        gtk_menu->attach_to_widget(event_box);
      }
    }
  }
  if (gtk_menu != nullptr) {
#if GTK_CHECK_VERSION(3, 22, 0)
    gtk_menu->popup_at_pointer(reinterpret_cast<GdkEvent*>(ev));
#else
    gtk_menu->popup(ev->button, ev->time);
#endif
    return true;
  }
  return false;
}

bool waybar::modules::SNI::Item::handleClick(GdkEventButton *const &ev) {
  auto parameters = VariantContainerBase::create_tuple({
    Variant<int>::create(ev->x),
    Variant<int>::create(ev->y)
  });
  if ((ev->button == 1 && item_is_menu) || ev->button == 3) {
    if (!makeMenu(ev)) {
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
