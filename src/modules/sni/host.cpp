#include "modules/sni/host.hpp"

#include <glibmm/main.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>

#include "util/scope_guard.hpp"

namespace waybar::modules::SNI {

Host::Host(const std::size_t id, const Json::Value& config, const Bar& bar,
           const std::function<void(std::unique_ptr<Item>&)>& on_add,
           const std::function<void(std::unique_ptr<Item>&)>& on_remove,
           const std::function<void()>& on_update)
    : bus_name_("org.kde.StatusNotifierHost-" + std::to_string(getpid()) + "-" +
                std::to_string(id)),
      object_path_("/StatusNotifierHost/" + std::to_string(id)),
      bus_name_id_(Gio::DBus::own_name(Gio::DBus::BusType::BUS_TYPE_SESSION, bus_name_,
                                       sigc::mem_fun(*this, &Host::busAcquired))),
      config_(config),
      bar_(bar),
      on_add_(on_add),
      on_remove_(on_remove),
      on_update_(on_update) {
  auto parse_list = [](const Json::Value& list,
                       std::unordered_map<std::string, std::size_t>& out) {
    if (!list.isArray()) return;
    for (Json::ArrayIndex i = 0; i < list.size(); ++i) {
      if (!list[i].isString()) continue;
      out.emplace(toLowerAscii(list[i].asString()), static_cast<std::size_t>(i));
    }
  };
  parse_list(config_["order-left"], order_left_);
  parse_list(config_["order-right"], order_right_);

  if (config_["reverse-direction"].isBool()) {
    reverse_direction_ = config_["reverse-direction"].asBool();
  }
}

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
  clearItems();
}

void Host::proxyReady(GObject* src, GAsyncResult* res, gpointer data) {
  GError* error = nullptr;
  waybar::util::ScopeGuard error_deleter([&error]() {
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
  waybar::util::ScopeGuard error_deleter([&error]() {
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
      host->removeItem(it);
      break;
    }
  }
}

void Host::itemReady(Item& item) {
  auto it = std::find_if(items_.begin(), items_.end(),
                         [&item](const auto& candidate) { return candidate.get() == &item; });
  if (it != items_.end() && (*it)->isReady()) {
    on_add_(*it);
    requestReorder();
  }
}

void Host::itemInvalidated(Item& item) {
  auto it = std::find_if(items_.begin(), items_.end(),
                         [&item](const auto& candidate) { return candidate.get() == &item; });
  if (it != items_.end()) {
    removeItem(it);
  }
}

void Host::removeItem(std::vector<std::unique_ptr<Item>>::iterator it) {
  if ((*it)->isReady()) {
    on_remove_(*it);
  }
  items_.erase(it);
}

void Host::clearItems() {
  bool removed_ready_item = false;
  for (auto& item : items_) {
    if (item->isReady()) {
      on_remove_(item);
      removed_ready_item = true;
    }
  }
  bool had_items = !items_.empty();
  items_.clear();
  if (had_items && !removed_ready_item) {
    on_update_();
  }
}

std::tuple<std::string, std::string> Host::getBusNameAndObjectPath(const std::string service) {
  auto it = service.find('/');
  if (it != std::string::npos) {
    return {service.substr(0, it), service.substr(it)};
  }
  return {service, "/StatusNotifierItem"};
}

void Host::addRegisteredItem(const std::string& service) {
  std::string bus_name, object_path;
  std::tie(bus_name, object_path) = getBusNameAndObjectPath(service);
  auto it = std::find_if(items_.begin(), items_.end(), [&bus_name, &object_path](const auto& item) {
    return bus_name == item->bus_name && object_path == item->object_path;
  });
  if (it == items_.end()) {
    items_.emplace_back(std::make_unique<Item>(
      bus_name, object_path, config_, bar_,
      [this](Item& item) { itemReady(item); },
      [this](Item& item) { itemInvalidated(item); },
      on_update_));
  }
}

std::string Host::toLowerAscii(std::string s) {
  for (auto& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

void Host::requestReorder() {
  if (reorder_pending_) return;
  reorder_pending_ = true;
  Glib::signal_idle().connect_once([this] {
    reorder_pending_ = false;
    reorderItems();
  });
}

void Host::reorderItems() {
  // Classify each item into one of three buckets: configured-left, configured-right, or middle
  // (alphabetical). Compute this once so the comparator stays cheap.
  enum class Bucket { Left, Middle, Right };
  struct Info {
    Bucket bucket;
    std::size_t cfg_index;  // only meaningful for Left/Right
    std::string key;        // lowercased sort_key, empty if none
  };

  std::unordered_map<Item*, Info> info;
  info.reserve(items_.size());
  for (const auto& it : items_) {
    const auto key = toLowerAscii(it->sort_key);
    auto left_it = order_left_.find(key);
    if (!key.empty() && left_it != order_left_.end()) {
      info[it.get()] = {Bucket::Left, left_it->second, key};
      continue;
    }
    auto right_it = order_right_.find(key);
    if (!key.empty() && right_it != order_right_.end()) {
      info[it.get()] = {Bucket::Right, right_it->second, key};
      continue;
    }
    info[it.get()] = {Bucket::Middle, 0, key};
  }

  std::stable_sort(items_.begin(), items_.end(),
                   [&info, this](const std::unique_ptr<Item>& a, const std::unique_ptr<Item>& b) {
                     const auto& ia = info[a.get()];
                     const auto& ib = info[b.get()];
                     if (ia.bucket != ib.bucket) {
                       return static_cast<int>(ia.bucket) < static_cast<int>(ib.bucket);
                     }
                     if (ia.bucket == Bucket::Left || ia.bucket == Bucket::Right) {
                       return ia.cfg_index < ib.cfg_index;
                     }
                     // Middle: alphabetical (optionally reversed)
                     if (ia.key != ib.key) {
                       return reverse_direction_ ? ia.key > ib.key : ia.key < ib.key;
                     }
                     return false;
                   });

  // Rebuild UI: remove all ready items, then re-add in sorted order.
  for (auto& it : items_) {
    if (it->isReady()) on_remove_(it);
  }
  for (auto& it : items_) {
    if (it->isReady()) on_add_(it);
  }
  on_update_();
}

}  // namespace waybar::modules::SNI
