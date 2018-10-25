#include "modules/sni/snh.hpp"

#include <iostream>

using namespace waybar::modules::SNI;

Host::Host(Glib::Dispatcher* dp)
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

void Host::busAcquired(GDBusConnection* connection,
  const gchar* name, gpointer data)
{
  auto host = static_cast<SNI::Host *>(data);
  host->watcher_id_ = g_bus_watch_name(
    G_BUS_TYPE_SESSION,
    "org.kde.StatusNotifierWatcher",
    G_BUS_NAME_WATCHER_FLAGS_NONE,
    &Host::nameAppeared, &Host::nameVanished, data, nullptr);
}

void Host::nameAppeared(GDBusConnection* connection,
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

void Host::nameVanished(GDBusConnection* connection,
  const gchar* name, gpointer data)
{
  auto host = static_cast<SNI::Host *>(data);
  g_cancellable_cancel(host->cancellable_);
  g_clear_object(&host->cancellable_);
  g_clear_object(&host->watcher_);
  host->items.clear();
}

void Host::proxyReady(GObject* src, GAsyncResult* res,
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

void Host::registerHost(GObject* src, GAsyncResult* res,
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

void Host::itemRegistered(
  SnOrgKdeStatusNotifierWatcher* watcher, const gchar* service, gpointer data)
{
  std::cout << "Item registered" << std::endl;
  auto host = static_cast<SNI::Host *>(data);
  host->addRegisteredItem(service);
}

void Host::itemUnregistered(
  SnOrgKdeStatusNotifierWatcher* watcher, const gchar* service, gpointer data)
{
  auto host = static_cast<SNI::Host *>(data);
  auto [bus_name, object_path] = host->getBusNameAndObjectPath(service);
  for (auto it = host->items.begin(); it != host->items.end(); ++it) {
    if (it->bus_name == bus_name && it->object_path == object_path) {
      host->items.erase(it);
      std::cout << "Item Unregistered" << std::endl;
      break;
    }
  }
  host->dp_->emit();
}

std::tuple<std::string, std::string> Host::getBusNameAndObjectPath(
  const gchar* service)
{
  std::string bus_name;
  std::string object_path;
  gchar* tmp = g_strstr_len(service, -1, "/");
  if (tmp != nullptr) {
    gchar** str = g_strsplit(service, "/", 2);
    bus_name = str[0];
    object_path = tmp;
    g_strfreev(str);
  } else {
    bus_name = service;
    object_path = "/StatusNotifierItem";
  }
  return { bus_name, object_path };
}

void Host::addRegisteredItem(const gchar* service)
{
  auto [bus_name, object_path] = getBusNameAndObjectPath(service);
  items.emplace_back(bus_name, object_path, dp_);
}
