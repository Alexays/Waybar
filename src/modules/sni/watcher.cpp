#include "modules/sni/watcher.hpp"

#include <spdlog/spdlog.h>

#include "util/scope_guard.hpp"

using namespace waybar::modules::SNI;

Watcher::Watcher()
    : bus_name_id_(Gio::DBus::own_name(Gio::DBus::BusType::BUS_TYPE_SESSION,
                                       "org.kde.StatusNotifierWatcher",
                                       sigc::mem_fun(*this, &Watcher::busAcquired),
                                       Gio::DBus::SlotNameAcquired(), Gio::DBus::SlotNameLost(),
                                       Gio::DBus::BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                           Gio::DBus::BUS_NAME_OWNER_FLAGS_REPLACE)),
      watcher_(sn_watcher_skeleton_new()) {}

Watcher::~Watcher() {
  if (hosts_ != nullptr) {
    g_slist_free_full(hosts_, gfWatchFree);
    hosts_ = nullptr;
  }
  if (items_ != nullptr) {
    g_slist_free_full(items_, gfWatchFree);
    items_ = nullptr;
  }
  Gio::DBus::unown_name(bus_name_id_);
  auto iface = G_DBUS_INTERFACE_SKELETON(watcher_);
  g_dbus_interface_skeleton_unexport(iface);
}

void Watcher::busAcquired(const Glib::RefPtr<Gio::DBus::Connection>& conn, Glib::ustring name) {
  GError* error = nullptr;
  waybar::util::ScopeGuard error_deleter([error]() {
    if (error) {
      g_error_free(error);
    }
  });
  g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(watcher_), conn->gobj(),
                                   "/StatusNotifierWatcher", &error);
  if (error != nullptr) {
    // Don't print an error when a watcher is already present
    if (error->code != 2) {
      spdlog::error("Watcher: {}", error->message);
    }
    return;
  }
  g_signal_connect_swapped(watcher_, "handle-register-item",
                           G_CALLBACK(&Watcher::handleRegisterItem), this);
  g_signal_connect_swapped(watcher_, "handle-register-host",
                           G_CALLBACK(&Watcher::handleRegisterHost), this);
}

gboolean Watcher::handleRegisterHost(Watcher* obj, GDBusMethodInvocation* invocation,
                                     const gchar* service) {
  const gchar* bus_name = service;
  const gchar* object_path = "/StatusNotifierHost";

  if (*service == '/') {
    bus_name = g_dbus_method_invocation_get_sender(invocation);
    object_path = service;
  }
  if (g_dbus_is_name(bus_name) == FALSE) {
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                          "D-Bus bus name '%s' is not valid", bus_name);
    return TRUE;
  }
  auto watch = gfWatchFind(obj->hosts_, bus_name, object_path);
  if (watch != nullptr) {
    g_warning("Status Notifier Host with bus name '%s' and object path '%s' is already registered",
              bus_name, object_path);
    sn_watcher_complete_register_item(obj->watcher_, invocation);
    return TRUE;
  }
  watch = gfWatchNew(GF_WATCH_TYPE_HOST, service, bus_name, object_path, obj);
  obj->hosts_ = g_slist_prepend(obj->hosts_, watch);
  if (!sn_watcher_get_is_host_registered(obj->watcher_)) {
    sn_watcher_set_is_host_registered(obj->watcher_, TRUE);
    sn_watcher_emit_host_registered(obj->watcher_);
  }
  sn_watcher_complete_register_host(obj->watcher_, invocation);
  return TRUE;
}

gboolean Watcher::handleRegisterItem(Watcher* obj, GDBusMethodInvocation* invocation,
                                     const gchar* service) {
  const gchar* bus_name = service;
  const gchar* object_path = "/StatusNotifierItem";

  if (*service == '/') {
    bus_name = g_dbus_method_invocation_get_sender(invocation);
    object_path = service;
  }
  if (g_dbus_is_name(bus_name) == FALSE) {
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                          "D-Bus bus name '%s' is not valid", bus_name);
    return TRUE;
  }
  auto watch = gfWatchFind(obj->items_, bus_name, object_path);
  if (watch != nullptr) {
    g_warning("Status Notifier Item with bus name '%s' and object path '%s' is already registered",
              bus_name, object_path);
    sn_watcher_complete_register_item(obj->watcher_, invocation);
    return TRUE;
  }
  watch = gfWatchNew(GF_WATCH_TYPE_ITEM, service, bus_name, object_path, obj);
  obj->items_ = g_slist_prepend(obj->items_, watch);
  obj->updateRegisteredItems(obj->watcher_);
  gchar* tmp = g_strdup_printf("%s%s", bus_name, object_path);
  sn_watcher_emit_item_registered(obj->watcher_, tmp);
  g_free(tmp);
  sn_watcher_complete_register_item(obj->watcher_, invocation);
  return TRUE;
}

Watcher::GfWatch* Watcher::gfWatchFind(GSList* list, const gchar* bus_name,
                                       const gchar* object_path) {
  for (GSList* l = list; l != nullptr; l = g_slist_next(l)) {
    auto watch = static_cast<GfWatch*>(l->data);
    if (g_strcmp0(watch->bus_name, bus_name) == 0 &&
        g_strcmp0(watch->object_path, object_path) == 0) {
      return watch;
    }
  }
  return nullptr;
}

void Watcher::gfWatchFree(gpointer data) {
  auto watch = static_cast<GfWatch*>(data);

  if (watch->watch_id > 0) {
    g_bus_unwatch_name(watch->watch_id);
  }

  g_free(watch->service);
  g_free(watch->bus_name);
  g_free(watch->object_path);

  g_free(watch);
}

Watcher::GfWatch* Watcher::gfWatchNew(GfWatchType type, const gchar* service, const gchar* bus_name,
                                      const gchar* object_path, Watcher* watcher) {
  GfWatch* watch = g_new0(GfWatch, 1);
  watch->type = type;
  watch->watcher = watcher;
  watch->service = g_strdup(service);
  watch->bus_name = g_strdup(bus_name);
  watch->object_path = g_strdup(object_path);
  watch->watch_id = g_bus_watch_name(G_BUS_TYPE_SESSION, bus_name, G_BUS_NAME_WATCHER_FLAGS_NONE,
                                     nullptr, &Watcher::nameVanished, watch, nullptr);
  return watch;
}

void Watcher::nameVanished(GDBusConnection* connection, const char* name, gpointer data) {
  auto watch = static_cast<GfWatch*>(data);
  if (watch->type == GF_WATCH_TYPE_HOST) {
    watch->watcher->hosts_ = g_slist_remove(watch->watcher->hosts_, watch);
    if (watch->watcher->hosts_ == nullptr) {
      sn_watcher_set_is_host_registered(watch->watcher->watcher_, FALSE);
      sn_watcher_emit_host_registered(watch->watcher->watcher_);
    }
  } else if (watch->type == GF_WATCH_TYPE_ITEM) {
    watch->watcher->items_ = g_slist_remove(watch->watcher->items_, watch);
    watch->watcher->updateRegisteredItems(watch->watcher->watcher_);
    gchar* tmp = g_strdup_printf("%s%s", watch->bus_name, watch->object_path);
    sn_watcher_emit_item_unregistered(watch->watcher->watcher_, tmp);
    g_free(tmp);
  }
}

void Watcher::updateRegisteredItems(SnWatcher* obj) {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
  for (GSList* l = items_; l != nullptr; l = g_slist_next(l)) {
    auto watch = static_cast<GfWatch*>(l->data);
    gchar* item = g_strdup_printf("%s%s", watch->bus_name, watch->object_path);
    g_variant_builder_add(&builder, "s", item);
    g_free(item);
  }
  GVariant* variant = g_variant_builder_end(&builder);
  const gchar** items = g_variant_get_strv(variant, nullptr);
  sn_watcher_set_registered_items(obj, items);
  g_variant_unref(variant);
  g_free(items);
}
