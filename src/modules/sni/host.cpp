#include "modules/sni/host.hpp"

#include <spdlog/spdlog.h>

#include "util/scope_guard.hpp"

namespace waybar::modules::SNI {

Host::Host(const std::size_t id, const Json::Value& config, const Bar& bar,
           const std::function<void(std::unique_ptr<Item>&)>& on_add,
           const std::function<void(std::unique_ptr<Item>&)>& on_remove)
    : bus_name_("org.kde.StatusNotifierHost-" + std::to_string(getpid()) + "-" +
                std::to_string(id)),
      object_path_("/StatusNotifierHost/" + std::to_string(id)),
      bus_name_id_(Gio::DBus::own_name(Gio::DBus::BusType::BUS_TYPE_SESSION, bus_name_,
                                       sigc::mem_fun(*this, &Host::busAcquired))),
      config_(config),
      bar_(bar),
      on_add_(on_add),
      on_remove_(on_remove) {}

Host::~Host() {
  if (bus_name_id_ > 0) {
    Gio::DBus::unown_name(bus_name_id_);
    bus_name_id_ = 0;
  }
  if (watcher_id_ > 0) {
    Gio::DBus::unwatch_name(watcher_id_);
    watcher_id_ = 0;
  }
  g_cancellable_cancel(cancellable_);
  g_clear_object(&cancellable_);
  g_clear_object(&watcher_);
}

void Host::busAcquired(const Glib::RefPtr<Gio::DBus::Connection>& conn, Glib::ustring name) {
  watcher_id_ = Gio::DBus::watch_name(conn, "org.kde.StatusNotifierWatcher",
                                      sigc::mem_fun(*this, &Host::nameAppeared),
                                      sigc::mem_fun(*this, &Host::nameVanished));
}

void Host::nameAppeared(const Glib::RefPtr<Gio::DBus::Connection>& conn, const Glib::ustring name,
                        const Glib::ustring& name_owner) {
  if (cancellable_ != nullptr) {
    // TODO
    return;
  }
  cancellable_ = g_cancellable_new();
  sn_watcher_proxy_new(conn->gobj(), G_DBUS_PROXY_FLAGS_NONE, "org.kde.StatusNotifierWatcher",
                       "/StatusNotifierWatcher", cancellable_, &Host::proxyReady, this);
}

void Host::nameVanished(const Glib::RefPtr<Gio::DBus::Connection>& conn, const Glib::ustring name) {
  g_cancellable_cancel(cancellable_);
  g_clear_object(&cancellable_);
  g_clear_object(&watcher_);
  items_.clear();
}

void Host::proxyReady(GObject* src, GAsyncResult* res, gpointer data) {
  GError* error = nullptr;
  waybar::util::ScopeGuard error_deleter([error]() {
    if (error != nullptr) {
      g_error_free(error);
    }
  });
  SnWatcher* watcher = sn_watcher_proxy_new_finish(res, &error);
  if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    spdlog::error("Host: {}", error->message);
    return;
  }
  auto host = static_cast<SNI::Host*>(data);
  host->watcher_ = watcher;
  if (error != nullptr) {
    spdlog::error("Host: {}", error->message);
    return;
  }
  sn_watcher_call_register_host(host->watcher_, host->object_path_.c_str(), host->cancellable_,
                                &Host::registerHost, data);
}

void Host::registerHost(GObject* src, GAsyncResult* res, gpointer data) {
  GError* error = nullptr;
  waybar::util::ScopeGuard error_deleter([error]() {
    if (error != nullptr) {
      g_error_free(error);
    }
  });
  sn_watcher_call_register_host_finish(SN_WATCHER(src), res, &error);
  if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    spdlog::error("Host: {}", error->message);
    return;
  }
  auto host = static_cast<SNI::Host*>(data);
  if (error != nullptr) {
    spdlog::error("Host: {}", error->message);
    return;
  }
  g_signal_connect(host->watcher_, "item-registered", G_CALLBACK(&Host::itemRegistered), data);
  g_signal_connect(host->watcher_, "item-unregistered", G_CALLBACK(&Host::itemUnregistered), data);
  auto items = sn_watcher_dup_registered_items(host->watcher_);
  if (items != nullptr) {
    for (uint32_t i = 0; items[i] != nullptr; i += 1) {
      host->addRegisteredItem(items[i]);
    }
  }
  g_strfreev(items);
}

void Host::itemRegistered(SnWatcher* watcher, const gchar* service, gpointer data) {
  auto host = static_cast<SNI::Host*>(data);
  host->addRegisteredItem(service);
}

void Host::itemUnregistered(SnWatcher* watcher, const gchar* service, gpointer data) {
  auto host = static_cast<SNI::Host*>(data);
  auto [bus_name, object_path] = host->getBusNameAndObjectPath(service);
  for (auto it = host->items_.begin(); it != host->items_.end(); ++it) {
    if ((*it)->bus_name == bus_name && (*it)->object_path == object_path) {
      host->on_remove_(*it);
      host->items_.erase(it);
      break;
    }
  }
}

std::tuple<std::string, std::string> Host::getBusNameAndObjectPath(const std::string service) {
  auto it = service.find('/');
  if (it != std::string::npos) {
    return {service.substr(0, it), service.substr(it)};
  }
  return {service, "/StatusNotifierItem"};
}

void Host::addRegisteredItem(std::string service) {
  std::string bus_name, object_path;
  std::tie(bus_name, object_path) = getBusNameAndObjectPath(service);
  auto it = std::find_if(items_.begin(), items_.end(), [&bus_name, &object_path](const auto& item) {
    return bus_name == item->bus_name && object_path == item->object_path;
  });
  if (it == items_.end()) {
    items_.emplace_back(new Item(bus_name, object_path, config_, bar_));
    on_add_(items_.back());
  }
}

}  // namespace waybar::modules::SNI
