#include "modules/sni/sni.hpp"

#include <iostream>

waybar::modules::SNI::Item::Item(std::string bus_name, std::string object_path,
  Glib::Dispatcher& dp)
  : icon_size(16), effective_icon_size(0),
    image(Gtk::manage(new Gtk::Image())),
    bus_name_(bus_name), object_path_(object_path), dp_(dp)
{
  cancellable_ = g_cancellable_new();
  sn_org_kde_status_notifier_item_proxy_new_for_bus(G_BUS_TYPE_SESSION,
    G_DBUS_PROXY_FLAGS_NONE, bus_name_.c_str(), object_path_.c_str(),
    cancellable_, &Item::proxyReady, this);
}

waybar::modules::SNI::Item::~Item()
{
}

void waybar::modules::SNI::Item::proxyReady(GObject* obj, GAsyncResult* res,
  gpointer data)
{
  GError* error = nullptr;
  SnOrgKdeStatusNotifierItem* proxy =
    sn_org_kde_status_notifier_item_proxy_new_for_bus_finish(res, &error);
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
  g_dbus_connection_call(conn, item->bus_name_.c_str(),
    item->object_path_.c_str(), "org.freedesktop.DBus.Properties", "GetAll",
    g_variant_new("(s)", "org.kde.StatusNotifierItem"),
    G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, -1,
    item->cancellable_, &Item::getAll, data);
}

void waybar::modules::SNI::Item::getAll(GObject* obj, GAsyncResult* res,
  gpointer data)
{
  GError* error = nullptr;
  auto conn = G_DBUS_CONNECTION(obj);
  GVariant* properties = g_dbus_connection_call_finish(conn, res, &error);
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
  GVariantIter* it = nullptr;
  g_variant_get(properties, "(a{sv})", &it);
  gchar* key;
  GVariant* value;
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
      item->window_id = g_variant_get_int32 (value);
    } else if (g_strcmp0(key, "IconName") == 0) {
      item->icon_name = g_variant_dup_string(value, nullptr);
    } else if (g_strcmp0(key, "IconPixmap") == 0) {
      // TODO: icon pixmap
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
    std::cerr << "Invalid Status Notifier Item: " + item->bus_name_ + ","
      + item->object_path_ << std::endl;
    return;
  }
  if (!item->icon_theme_path.empty()) {
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    gtk_icon_theme_append_search_path(icon_theme,
      item->icon_theme_path.c_str());
  }
  item->updateImage();
  item->dp_.emit();
  // TODO: handle change
}

void waybar::modules::SNI::Item::updateImage()
{
  if (!icon_name.empty()) {
    auto pixbuf = getIconByName(icon_name, icon_size);
    if (pixbuf->gobj() == nullptr) {
      // Try to find icons specified by path and filename
      pixbuf = Gdk::Pixbuf::create_from_file(icon_name);
      if (pixbuf->gobj() != nullptr) {
        // An icon specified by path and filename may be the wrong size for the tray
        pixbuf->scale_simple(icon_size - 2, icon_size - 2,
          Gdk::InterpType::INTERP_BILINEAR);
      }
    }
    if (pixbuf->gobj() == nullptr) {
      pixbuf = getIconByName("image-missing", icon_size);
    }
    image->set(pixbuf);
  } else {
    image->set_from_icon_name("image-missing", Gtk::ICON_SIZE_MENU);
    image->set_pixel_size(icon_size);
  }
}

Glib::RefPtr<Gdk::Pixbuf> waybar::modules::SNI::Item::getIconByName(
  std::string name, int request_size)
{
  int icon_size = 0;
  Glib::RefPtr<Gtk::IconTheme> icon_theme =
    Gtk::IconTheme::get_default();
  icon_theme->rescan_if_needed();
  auto sizes = icon_theme->get_icon_sizes(name.c_str());
  for (auto size : sizes) {
    // -1 == scalable
    if (size == request_size || size == -1) {
      icon_size = request_size;
      break;
    } else if (size < request_size || size > icon_size) {
      icon_size = size;
    }
  }
  if (icon_size == 0) {
    icon_size = request_size;
  }
  return icon_theme->load_icon(name.c_str(), icon_size,
    Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE);
}