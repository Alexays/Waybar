#include "modules/sni/item.hpp"

#include <iostream>

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
  cancellable_ = g_cancellable_new();
  sn_item_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, bus_name.c_str(),
      object_path.c_str(), cancellable_, &Item::proxyReady, this);
}

void waybar::modules::SNI::Item::proxyReady(GObject *obj, GAsyncResult *res,
                                            gpointer data) {
  GError *error = nullptr;
  SnItem *proxy = sn_item_proxy_new_for_bus_finish(res, &error);
  if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    g_error_free(error);
    return;
  }
  auto item = static_cast<SNI::Item *>(data);
  item->proxy_ = proxy;
  if (error) {
    std::cerr << error->message << std::endl;
    g_error_free(error);
    return;
  }
  auto conn = g_dbus_proxy_get_connection(G_DBUS_PROXY(proxy));

  g_dbus_connection_call(conn, item->bus_name.c_str(),
    item->object_path.c_str(), "org.freedesktop.DBus.Properties", "GetAll",
    g_variant_new("(s)", "org.kde.StatusNotifierItem"),
    G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, -1, item->cancellable_,
    &Item::getAll, data);
}

void waybar::modules::SNI::Item::getAll(GObject *obj, GAsyncResult *res,
                                        gpointer data) {
  GError *error = nullptr;
  auto conn = G_DBUS_CONNECTION(obj);
  GVariant *properties = g_dbus_connection_call_finish(conn, res, &error);
  if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    g_error_free(error);
    return;
  }
  auto item = static_cast<SNI::Item *>(data);
  if (error) {
    std::cerr << error->message << std::endl;
    g_error_free(error);
    return;
  }
  GVariantIter *it = nullptr;
  g_variant_get(properties, "(a{sv})", &it);
  gchar *key;
  GVariant *value;
  while (g_variant_iter_next(it, "{sv}", &key, &value)) {
    if (g_strcmp0(key, "Category") == 0) {
      item->category = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "Id") == 0) {
      item->id = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "Title") == 0) {
      item->title = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "Status") == 0) {
      item->status = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "WindowId") == 0) {
      item->window_id = g_variant_get_int32(value);
    } else if (g_strcmp0(key, "IconName") == 0) {
      item->icon_name = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "IconPixmap") == 0) {
      item->icon_pixmap = item->extractPixBuf(value);
    } else if (g_strcmp0(key, "OverlayIconName") == 0) {
      item->overlay_icon_name = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "OverlayIconPixmap") == 0) {
      // TODO: overlay_icon_pixmap
    } else if (g_strcmp0(key, "AttentionIconName") == 0) {
      item->attention_icon_name = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "AttentionIconPixmap") == 0) {
      // TODO: attention_icon_pixmap
    } else if (g_strcmp0(key, "AttentionMovieName") == 0) {
      item->attention_movie_name = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "ToolTip") == 0) {
      // TODO: tooltip
    } else if (g_strcmp0(key, "IconThemePath") == 0) {
      item->icon_theme_path = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "Menu") == 0) {
      item->menu = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "ItemIsMenu") == 0) {
      item->item_is_menu = g_variant_get_boolean(value);
    }
    g_variant_unref(value);
    g_free(key);
  }
  g_variant_iter_free(it);
  g_variant_unref(properties);
  if (item->id.empty() || item->category.empty() || item->status.empty()) {
    std::cerr << "Invalid Status Notifier Item: " + item->bus_name + "," +
      item->object_path << std::endl;
    return;
  }
  if (!item->icon_theme_path.empty()) {
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    gtk_icon_theme_append_search_path(icon_theme,
      item->icon_theme_path.c_str());
  }
  item->updateImage();
  // item->event_box.set_tooltip_text(item->title);
  // TODO: handle change
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
                                         true, 8, lwidth, lheight, 4 * lwidth);
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
    } else if (size < request_size || size > tmp_size) {
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
  if ((ev->button == 1 && item_is_menu) || ev->button == 3) {
    if (!makeMenu(ev)) {
      return sn_item_call_context_menu_sync(proxy_, ev->x, ev->y, nullptr, nullptr);
    }
  } else if (ev->button == 1) {
    return sn_item_call_activate_sync(proxy_, ev->x, ev->y, nullptr, nullptr);
  } else if (ev->button == 2) {
    return sn_item_call_secondary_activate_sync(proxy_, ev->x, ev->y,
      nullptr, nullptr);
  }
  return false;
}
