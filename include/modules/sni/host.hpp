#pragma once

#include <dbus-status-notifier-watcher.h>
#include <giomm.h>
#include <glibmm/refptr.h>
#include <json/json.h>

#include <tuple>

#include "bar.hpp"
#include "modules/sni/item.hpp"

namespace waybar::modules::SNI {

class Host {
 public:
  Host(const std::size_t id, const Json::Value&, const Bar&,
       const std::function<void(std::unique_ptr<Item>&)>&,
       const std::function<void(std::unique_ptr<Item>&)>&);
  ~Host();

 private:
  void busAcquired(const Glib::RefPtr<Gio::DBus::Connection>&, Glib::ustring);
  void nameAppeared(const Glib::RefPtr<Gio::DBus::Connection>&, Glib::ustring,
                    const Glib::ustring&);
  void nameVanished(const Glib::RefPtr<Gio::DBus::Connection>&, Glib::ustring);
  static void proxyReady(GObject*, GAsyncResult*, gpointer);
  static void registerHost(GObject*, GAsyncResult*, gpointer);
  static void itemRegistered(SnWatcher*, const gchar*, gpointer);
  static void itemUnregistered(SnWatcher*, const gchar*, gpointer);

  std::tuple<std::string, std::string> getBusNameAndObjectPath(const std::string);
  void addRegisteredItem(std::string service);

  std::vector<std::unique_ptr<Item>> items_;
  const std::string bus_name_;
  const std::string object_path_;
  std::size_t bus_name_id_;
  std::size_t watcher_id_;
  GCancellable* cancellable_ = nullptr;
  SnWatcher* watcher_ = nullptr;
  const Json::Value& config_;
  const Bar& bar_;
  const std::function<void(std::unique_ptr<Item>&)> on_add_;
  const std::function<void(std::unique_ptr<Item>&)> on_remove_;
};

}  // namespace waybar::modules::SNI
