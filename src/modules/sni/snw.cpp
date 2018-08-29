#include "modules/sni/snw.hpp"

#include <iostream>

waybar::modules::SNI::Watcher::Watcher()
{
  GBusNameOwnerFlags flags = static_cast<GBusNameOwnerFlags>(
    G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT
    | G_BUS_NAME_OWNER_FLAGS_REPLACE);
  bus_name_id_ = g_bus_own_name(G_BUS_TYPE_SESSION,
    "org.kde.StatusNotifierWatcher", flags,
    &Watcher::busAcquired, nullptr, nullptr, this, nullptr);
  watcher_ = sn_org_kde_status_notifier_watcher_skeleton_new();
}

waybar::modules::SNI::Watcher::~Watcher()
{
}

void waybar::modules::SNI::Watcher::busAcquired(GDBusConnection* connection,
  const gchar* name, gpointer data)
{
  GError* error = nullptr;
  auto host = static_cast<SNI::Watcher*>(data);
  g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(host->watcher_),
    connection, "/StatusNotifierWatcher", &error);
  if (error != nullptr) {
    std::cerr << error->message << std::endl;
    g_error_free(error);
    return;
  }
  g_signal_connect_swapped(host->watcher_,
    "handle-register-status-notifier-item",
    G_CALLBACK(&Watcher::handleRegisterItem), data);
  g_signal_connect_swapped(host->watcher_,
    "handle-register-status-notifier-host",
    G_CALLBACK(&Watcher::handleRegisterHost), data);
  sn_org_kde_status_notifier_watcher_set_protocol_version(host->watcher_, 0);
  sn_org_kde_status_notifier_watcher_set_is_status_notifier_host_registered(
    host->watcher_, TRUE);
  std::cout << "Bus aquired" << std::endl;
}

gboolean waybar::modules::SNI::Watcher::handleRegisterHost(
  Watcher* obj, GDBusMethodInvocation* invocation,
  const gchar* service)
{
  const gchar* bus_name = service;
  const gchar* object_path = "/StatusNotifierHost";

  if (*service == '/') {
    bus_name = g_dbus_method_invocation_get_sender(invocation);
    object_path = service;
  }
  if (g_dbus_is_name(bus_name) == FALSE) {
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
      G_DBUS_ERROR_INVALID_ARGS, "D-Bus bus name '%s' is not valid", bus_name);
    return TRUE;
  }
  auto watch = gfWatchFind(obj->hosts_, bus_name, object_path);
  if (watch != nullptr) {
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
      G_DBUS_ERROR_INVALID_ARGS, "Status Notifier Host with bus name '%s' and object path '%s' is already registered",
      bus_name, object_path);
    return TRUE;
  }
  watch = gfWatchNew(GF_WATCH_TYPE_HOST, service, bus_name, object_path);
  obj->hosts_ = g_slist_prepend(obj->hosts_, watch);
  sn_org_kde_status_notifier_watcher_set_is_status_notifier_host_registered(
    obj->watcher_, TRUE);
  if (g_slist_length(obj->hosts_)) {
    sn_org_kde_status_notifier_watcher_emit_status_notifier_host_registered(
      obj->watcher_);
  }
  sn_org_kde_status_notifier_watcher_complete_register_status_notifier_host(
    obj->watcher_, invocation);
  std::cout << "Host registered: " << bus_name << std::endl;
  return TRUE;
}

gboolean waybar::modules::SNI::Watcher::handleRegisterItem(
  Watcher* obj, GDBusMethodInvocation* invocation,
  const gchar* service)
{
  const gchar* bus_name = service;
  const gchar* object_path = "/StatusNotifierItem";

  if (*service == '/') {
    bus_name = g_dbus_method_invocation_get_sender(invocation);
    object_path = service;
  }
  if (g_dbus_is_name(bus_name) == FALSE) {
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
      G_DBUS_ERROR_INVALID_ARGS, "D-Bus bus name '%s' is not valid", bus_name);
    return TRUE;
  }
  auto watch = gfWatchFind(obj->items_, bus_name, object_path);
  if (watch != nullptr) {
    g_warning("Status Notifier Item with bus name '%s' and object path '%s' is already registered",
      bus_name, object_path);
    sn_org_kde_status_notifier_watcher_complete_register_status_notifier_item(
      obj->watcher_, invocation);
    return TRUE;
  }
  watch = gfWatchNew(GF_WATCH_TYPE_ITEM, service, bus_name, object_path);
  obj->items_ = g_slist_prepend(obj->items_, watch);
  obj->updateRegisteredItems(obj->watcher_);
  gchar* tmp = g_strdup_printf("%s%s", bus_name, object_path);
  sn_org_kde_status_notifier_watcher_emit_status_notifier_item_registered(
    obj->watcher_, tmp);
  g_free(tmp);
  sn_org_kde_status_notifier_watcher_complete_register_status_notifier_item(
    obj->watcher_, invocation);
  return TRUE;
}

waybar::modules::SNI::GfWatch* waybar::modules::SNI::Watcher::gfWatchFind(
  GSList* list, const gchar* bus_name, const gchar* object_path)
{
  for (GSList* l = list; l != nullptr; l = g_slist_next (l)) {
    GfWatch* watch = static_cast<GfWatch*>(l->data);
    if (g_strcmp0 (watch->bus_name, bus_name) == 0
      && g_strcmp0 (watch->object_path, object_path) == 0) {
      return watch;
    }
  }
  return nullptr;
}

waybar::modules::SNI::GfWatch* waybar::modules::SNI::Watcher::gfWatchNew(
  GfWatchType type, const gchar* service, const gchar* bus_name,
  const gchar* object_path)
{
  GfWatch* watch = g_new0(GfWatch, 1);
  watch->type = type;
  watch->service = g_strdup(service);
  watch->bus_name = g_strdup(bus_name);
  watch->object_path = g_strdup(object_path);
  watch->watch_id = g_bus_watch_name(G_BUS_TYPE_SESSION, bus_name,
    G_BUS_NAME_WATCHER_FLAGS_NONE, nullptr, &Watcher::nameVanished, watch,
    nullptr);
  return watch;
}

void waybar::modules::SNI::Watcher::nameVanished(GDBusConnection* connection,
  const char* name, gpointer data)
{
  //TODO
  std::cout << "name vanished" << std::endl;
}

void waybar::modules::SNI::Watcher::updateRegisteredItems(
  SnOrgKdeStatusNotifierWatcher* obj)
{
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("as")); 
  for (GSList* l = items_; l != nullptr; l = g_slist_next(l)) {
    GfWatch* watch = static_cast<GfWatch*>(l->data);
    gchar* item = g_strdup_printf ("%s%s", watch->bus_name, watch->object_path);
    g_variant_builder_add (&builder, "s", item);
    g_free (item);
  }
  GVariant* variant = g_variant_builder_end(&builder);
  const gchar** items = g_variant_get_strv (variant, nullptr);
  sn_org_kde_status_notifier_watcher_set_registered_status_notifier_items(
    obj, items);
  g_variant_unref(variant);
  g_free(items);
}