#include "modules/sni/tray.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

#include "modules/sni/icon_manager.hpp"

namespace waybar::modules::SNI {

static void initIconsConfig(const Json::Value& config) {
  if (config["icons"].isObject()) {
    IconManager::instance().setIconsConfig(config["icons"]);
  }
}

std::vector<std::string> Tray::parseIgnoreList(const Json::Value& config) {
  std::vector<std::string> ignore_list;
  if (config["ignore-list"].isArray()) {
    spdlog::info("Tray: Found ignore-list with {} items", config["ignore-list"].size());
    for (const auto& item : config["ignore-list"]) {
      if (item.isString()) {
        ignore_list.push_back(item.asString());
        spdlog::info("Tray: Adding to ignore list: {}", item.asString());
      }
    }
  } else {
    spdlog::info("Tray: No ignore-list configured");
  }
  return ignore_list;
}

Tray::Tray(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "tray", id),
      box_(bar.orientation, 0),
      watcher_(SNI::Watcher::getInstance()),
      ignore_list_(parseIgnoreList(config)),
      host_((initIconsConfig(config), nb_hosts_), config, bar, ignore_list_,
            std::bind(&Tray::onAdd, this, std::placeholders::_1),
            std::bind(&Tray::onRemove, this, std::placeholders::_1),
            std::bind(&Tray::reorderBox, this), std::bind(&Tray::queueUpdate, this)) {
  w_ = &box_;
  box_.set_name("tray");
  if (!id.empty()) {
    box_.add_css_class(id);
  }
  box_.add_css_class(MODULE_CLASS);
  if (config_["spacing"].isUInt()) {
    box_.set_spacing(config_["spacing"].asUInt());
  }
  nb_hosts_ += 1;
}

void Tray::checkIgnoreList(std::unique_ptr<Item>* item_ptr) {
  // Delegate to Host's checkIgnoreList method
  host_.checkIgnoreList(ignore_list_, std::bind(&Tray::onRemove, this, std::placeholders::_1));
}

void Tray::queueUpdate() { dp.emit(); }

void Tray::onAdd(std::unique_ptr<Item>& item) {
  spdlog::info("Tray::onAdd - item bus_name='{}', category='{}', icon_name='{}', title='{}'",
               item->bus_name, item->category, item->icon_name, item->title);

  // Idempotency guard: onAdd can be reached more than once for the same item
  // (e.g. an item is processed while its Id/order is resolved and then again
  // when it becomes ready). Re-adding would re-parent the event_box (GTK
  // "widget already has a parent" critical), push a duplicate pointer into
  // items_ and leak extra signal connections.
  if (std::find(items_.begin(), items_.end(), item.get()) != items_.end()) {
    spdlog::debug("Tray::onAdd - item already added, skipping");
    return;
  }

  if (config_["reverse-direction"].isBool() && config_["reverse-direction"].asBool()) {
    box_.append(item->widget_);
  } else {
    box_.prepend(item->widget_);
  }
  items_.push_back(item.get());

  auto show_conn = item->widget_.signal_show().connect([this] { dp.emit(); });
  auto hide_conn = item->widget_.signal_hide().connect([this] { dp.emit(); });
  item_connections_[item.get()] = {show_conn, hide_conn};

  // Position the freshly added widget according to the configured order. This
  // must happen before the ignore-list check below, which may erase `item`.
  reorderBox();

  // After this point `item` may be erased/invalidated by the ignore-list check;
  // do not touch it again below.
  spdlog::debug("Tray::onAdd deferred check - checking ignore list");
  host_.checkIgnoreList(ignore_list_, std::bind(&Tray::onRemove, this, std::placeholders::_1));

  dp.emit();
}

void Tray::onRemove(std::unique_ptr<Item>& item) {
  // May be called for items that were never added (e.g. the ignore-list check
  // runs over items that are not yet ready). Only touch state we actually own.
  auto it = std::find(items_.begin(), items_.end(), item.get());
  if (it == items_.end()) {
    return;
  }

  auto conn_it = item_connections_.find(item.get());
  if (conn_it != item_connections_.end()) {
    conn_it->second.first.disconnect();
    conn_it->second.second.disconnect();
    item_connections_.erase(conn_it);
  }

  box_.remove(item->widget_);
  items_.erase(it);
  dp.emit();
}

void Tray::reorderBox() {
  const bool reverse =
      config_["reverse-direction"].isBool() && config_["reverse-direction"].asBool();
  // Stable sort keeps insertion order among items sharing the same order value.
  std::stable_sort(items_.begin(), items_.end(),
                   [](const Item* a, const Item* b) { return a->order_ < b->order_; });
  for (auto* item : items_) {
    box_.remove(item->widget_);
  }
  if (reverse) {
    for (auto it = items_.rbegin(); it != items_.rend(); ++it) {
      box_.append((*it)->widget_);
    }
  } else {
    for (auto* item : items_) {
      box_.append(item->widget_);
    }
  }
}

auto Tray::doUpdate() -> void {
  // Check if any items should be ignored now that properties have loaded
  if (!ignore_list_.empty()) {
    spdlog::debug("Tray::update() - checking ignore list");
    host_.checkIgnoreList(ignore_list_, std::bind(&Tray::onRemove, this, std::placeholders::_1));
  }

  // Show tray only when items are visible. Iterate the managed items_ list
  // instead of box_.get_children() to avoid a use-after-free on raw widget
  // pointers that may dangle after items are destroyed asynchronously.
  box_.set_visible(std::any_of(items_.begin(), items_.end(),
                               [](Item* item) { return item->widget_.get_visible(); }));
  AModule::doUpdate();
}

}  // namespace waybar::modules::SNI
