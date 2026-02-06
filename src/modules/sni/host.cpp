#include "modules/sni/host.hpp"

#include <spdlog/spdlog.h>

#include "util/scope_guard.hpp"

#include <algorithm>
#include <cctype>
#include <glibmm/main.h>
#include <unordered_set>

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
      on_remove_(on_remove) {
  // Parse "order" list: map key -> index (0..n-1)
  order_list_.clear();
  if (config_["order"].isArray()) {
    order_list_.reserve(config_["order"].size());
    for (Json::ArrayIndex i = 0; i < config_["order"].size(); ++i) {
      const auto& v = config_["order"][i];
      if (!v.isString()) continue;
      auto key = toLowerAscii(v.asString());
      order_list_.push_back(key);
      order_index_[key] = static_cast<std::size_t>(order_list_.size() - 1);
    }
  }

  // Unknown placement: "after" (default) or "before"
  if (config_["order-unknown"].isString()) {
    const auto s = toLowerAscii(config_["order-unknown"].asString());
    if (s == "before") unknown_after_ = false;
    else if (s == "after") unknown_after_ = true;
  }

  if (!order_list_.empty()) {
    std::string line = "tray: configured order:";
    for (const auto& k : order_list_) {
      line += " [";
      line += k;
      line += "]";
    }
    line += unknown_after_ ? " unknown=after" : " unknown=before";
    spdlog::info("{}", line);
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
    items_.emplace_back(new Item(*this, bus_name, object_path, config_, bar_));
    seq_[items_.back().get()] = next_seq_++;
    on_add_(items_.back());
  }
}

std::string Host::toLowerAscii(std::string s) {
  for (auto& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

void Host::requestReorder() {
  if (reorder_pending_) {
    return;
  }
  reorder_pending_ = true;

  Glib::signal_idle().connect_once([this]() {
    this->reorderItems();
    this->reorder_pending_ = false;
  });
}

void Host::reorderItems() {
  // 1) Remove all items from UI
  for (auto& it : items_) {
    on_remove_(it);
  }

  // 2) Sort canonical item storage by sort_key (stable)
  std::stable_sort(items_.begin(), items_.end(),
    [this](const std::unique_ptr<Item>& a, const std::unique_ptr<Item>& b) {

      const auto& ka_raw = a->sort_key;
      const auto& kb_raw = b->sort_key;
      const bool a_has = !ka_raw.empty();
      const bool b_has = !kb_raw.empty();

      // keep empty keys deterministic
      if (a_has != b_has) return a_has;

      const auto ka = a_has ? toLowerAscii(ka_raw) : std::string{};
      const auto kb = b_has ? toLowerAscii(kb_raw) : std::string{};

      const auto ia = a_has ? order_index_.find(ka) : order_index_.end();
      const auto ib = b_has ? order_index_.find(kb) : order_index_.end();

      const bool a_cfg = (ia != order_index_.end());
      const bool b_cfg = (ib != order_index_.end());

      if (a_cfg != b_cfg) {
        // unknown_after_==true  -> configured first
        // unknown_after_==false -> unknown first
        return unknown_after_ ? a_cfg : !a_cfg;
      }

      if (a_cfg && b_cfg) {
        if (ia->second != ib->second) return ia->second < ib->second;
        // fall through to alpha/seq
      }

      if (a_has && b_has && ka != kb) return ka < kb;

      return seq_[a.get()] < seq_[b.get()];
    });

  // 3) Add all items back to UI in a way that matches Tray's packing direction.
  const bool reverse =
      config_["reverse-direction"].isBool() && config_["reverse-direction"].asBool();

  // IMPORTANT:
  // Tray::onAdd() uses pack_start when reverse-direction is false. If we add items
  // in sorted order with pack_start, the visual order is reversed. Therefore we
  // iterate backwards in that case.
  if (!reverse) {
    for (auto& it : items_) {
      on_add_(it);
    }
  } else {
    for (auto it = items_.rbegin(); it != items_.rend(); ++it) {
      on_add_(*it);
    }
  }
  {
    std::string line;
    line.reserve(256);
    line += "tray: host sorted order:";
    for (const auto& it : items_) {
      const auto& key = it->sort_key;
      const auto seq_it = seq_.find(it.get());
      const auto seq = (seq_it != seq_.end()) ? seq_it->second : 999999u;

      line += " [";
      line += key.empty() ? "<empty>" : key;
      line += "#";
      line += std::to_string(seq);
      line += "]";
    }
    spdlog::info("{}", line);
  }

  if (!order_list_.empty()) {
    std::unordered_set<std::string> seen;
    seen.reserve(items_.size());

    for (const auto& it : items_) {
      if (!it->sort_key.empty()) {
        seen.insert(toLowerAscii(it->sort_key));
      }
    }

    std::string found = "tray: order found:";
    std::string missing = "tray: order missing:";

    bool any_found = false;
    bool any_missing = false;

    for (const auto& k : order_list_) {
      if (seen.find(k) != seen.end()) {
        found += " [";
        found += k;
        found += "]";
        any_found = true;
      } else {
        missing += " [";
        missing += k;
        missing += "]";
        any_missing = true;
      }
    }

    if (any_found) spdlog::info("{}", found);
    if (any_missing) spdlog::info("{}", missing);
  }
}

}  // namespace waybar::modules::SNI
