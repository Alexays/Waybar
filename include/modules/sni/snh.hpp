#pragma once

#include <gtkmm.h>

#include <tuple>
#include <dbus-status-notifier-watcher.h>
#include "modules/sni/sni.hpp"

namespace waybar::modules::SNI {

class Host {
  public:
    Host(Glib::Dispatcher*);
    std::vector<Item> items;
  private:
    static void busAcquired(GDBusConnection*, const gchar*, gpointer);
    static void nameAppeared(GDBusConnection*, const gchar*, const gchar*,
      gpointer);
    static void nameVanished(GDBusConnection*, const gchar*, gpointer);
    static void proxyReady(GObject*, GAsyncResult*, gpointer);
    static void registerHost(GObject*, GAsyncResult*, gpointer);
    static void itemRegistered(SnOrgKdeStatusNotifierWatcher*, const gchar*,
      gpointer);
    static void itemUnregistered(SnOrgKdeStatusNotifierWatcher*, const gchar*,
      gpointer);

    std::tuple<std::string, std::string> getBusNameAndObjectPath(const gchar*);
    void addRegisteredItem(const gchar* service);

    uint32_t bus_name_id_;
    uint32_t watcher_id_;
    std::string bus_name_;
    std::string object_path_;
    Glib::Dispatcher* dp_;
    GCancellable* cancellable_ = nullptr;
    SnOrgKdeStatusNotifierWatcher* watcher_ = nullptr;
};

}
