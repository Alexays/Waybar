#include "modules/sni/snh.hpp"

#include <iostream>

waybar::modules::SNI::Host::Host(Glib::Dispatcher& dp)
: dp_(dp)
{
  GBusNameOwnerFlags flags = static_cast<GBusNameOwnerFlags>(
    G_BUS_NAME_OWNER_FLAGS_NONE);
  bus_name_ = "org.kde.StatusNotifierHost-" + std::to_string(getpid());
  object_path_ = "/StatusNotifierHost";
  bus_name_id_ = g_bus_own_name(G_BUS_TYPE_SESSION,
    bus_name_.c_str(), flags,
    &Host::busAcquired, nullptr, nullptr, this, nullptr);
}

waybar::modules::SNI::Host::~Host()
{
}

void waybar::modules::SNI::Host::busAcquired(GDBusConnection* connection,
  const gchar* name, gpointer data)
{
  auto host = static_cast<SNI::Host *>(data);
  host->watcher_id_ = g_bus_watch_name(
    G_BUS_TYPE_SESSION,
    "org.kde.StatusNotifierWatcher",
    G_BUS_NAME_WATCHER_FLAGS_NONE,
    &Host::nameAppeared, &Host::nameVanished, data, nullptr);
}

void waybar::modules::SNI::Host::nameAppeared(GDBusConnection* connection,
  const gchar* name, const gchar* name_owner, gpointer data)
{
  auto host = static_cast<SNI::Host *>(data);
  if (host->cancellable_ != nullptr) {
    std::cout << "WTF" << std::endl;
  }
  host->cancellable_ = g_cancellable_new();
  sn_org_kde_status_notifier_watcher_proxy_new(
    connection,
    G_DBUS_PROXY_FLAGS_NONE,
    "org.kde.StatusNotifierWatcher",
    "/StatusNotifierWatcher",
    host->cancellable_, &Host::proxyReady, data);
}

void waybar::modules::SNI::Host::nameVanished(GDBusConnection* connection,
  const gchar* name, gpointer data)
{
  auto host = static_cast<SNI::Host *>(data);
  g_cancellable_cancel(host->cancellable_);
  g_clear_object(&host->cancellable_);
  g_clear_object(&host->watcher_);
  host->items.clear();
}

void waybar::modules::SNI::Host::proxyReady(GObject* src, GAsyncResult* res,
  gpointer data)
{
  GError* error = nullptr;
  SnOrgKdeStatusNotifierWatcher* watcher =
    sn_org_kde_status_notifier_watcher_proxy_new_finish(res, &error);
  if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    std::cerr << error->message << std::endl;
    g_error_free(error);
    return;
  }
  auto host = static_cast<SNI::Host *>(data);
  host->watcher_ = watcher;
  if (error != nullptr) {
    std::cerr << error->message << std::endl;
    g_error_free(error);
    return;
  }
  sn_org_kde_status_notifier_watcher_call_register_status_notifier_host(
    host->watcher_, host->object_path_.c_str(), host->cancellable_,
    &Host::registerHost, data);
}

void waybar::modules::SNI::Host::registerHost(GObject* src, GAsyncResult* res,
  gpointer data)
{
  GError* error = nullptr;
  sn_org_kde_status_notifier_watcher_call_register_status_notifier_host_finish(
    SN_ORG_KDE_STATUS_NOTIFIER_WATCHER(src), res, &error);
  if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    std::cerr << error->message << std::endl;
    g_error_free(error);
    return;
  }
  auto host = static_cast<SNI::Host *>(data);
  if (error != nullptr) {
    std::cerr << error->message << std::endl;
    g_error_free(error);
    return; 
  }
  g_signal_connect(host->watcher_, "status-notifier-item-registered",
    G_CALLBACK(&Host::itemRegistered), data);
  g_signal_connect(host->watcher_, "status-notifier-item-unregistered",
    G_CALLBACK(&Host::itemUnregistered), data);
  auto items =
    sn_org_kde_status_notifier_watcher_dup_registered_status_notifier_items(host->watcher_);
  if (items) {
    for (uint32_t i = 0; items[i] != nullptr; i += 1) {
      host->addRegisteredItem(items[i]);
    }
  }
  g_strfreev(items);
}

void waybar::modules::SNI::Host::itemRegistered(
  SnOrgKdeStatusNotifierWatcher* watcher, const gchar* service, gpointer data)
{
  std::cout << "Item registered" << std::endl;
  auto host = static_cast<SNI::Host *>(data);
  host->addRegisteredItem(service);
}

void waybar::modules::SNI::Host::itemUnregistered(
  SnOrgKdeStatusNotifierWatcher* watcher, const gchar* service, gpointer data)
{
  std::cout << "Item Unregistered" << std::endl;
}

void waybar::modules::SNI::Host::getBusNameAndObjectPath(const gchar* service,
  gchar** bus_name, gchar** object_path)
{
  gchar* tmp = g_strstr_len (service, -1, "/");
  if (tmp != nullptr) {
    gchar** str = g_strsplit(service, "/", 2);
    *bus_name = g_strdup(str[0]);
    *object_path = g_strdup(tmp);
    g_strfreev(str);
  } else {
    *bus_name = g_strdup(service);
    *object_path = g_strdup("/StatusNotifierItem");
  }
}

void waybar::modules::SNI::Host::addRegisteredItem(const gchar* service)
{
  gchar* bus_name = nullptr;
  gchar* object_path = nullptr;

  getBusNameAndObjectPath(service, &bus_name, &object_path);
  items.emplace_back(bus_name, object_path, dp_);
}