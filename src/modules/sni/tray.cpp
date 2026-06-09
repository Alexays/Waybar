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
            std::bind(&Tray::queueUpdate, this)) {
  box_.set_name("tray");
  event_box_.add(box_);
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  if (config_["spacing"].isUInt()) {
    box_.set_spacing(config_["spacing"].asUInt());
  }
  nb_hosts_ += 1;
  dp.emit();
}

void Tray::checkIgnoreList(std::unique_ptr<Item>* item_ptr) {
  // Delegate to Host's checkIgnoreList method
  host_.checkIgnoreList(ignore_list_, std::bind(&Tray::onRemove, this, std::placeholders::_1));
}

void Tray::queueUpdate() { dp.emit(); }

void Tray::onAdd(std::unique_ptr<Item>& item) {
  spdlog::info("Tray::onAdd - item bus_name='{}', category='{}', icon_name='{}', title='{}'",
               item->bus_name, item->category, item->icon_name, item->title);

  if (config_["reverse-direction"].isBool() && config_["reverse-direction"].asBool()) {
    box_.pack_end(item->event_box);
  } else {
    box_.pack_start(item->event_box);
  }
  items_.push_back(item.get());

  spdlog::debug("Tray::onAdd deferred check - checking ignore list");
  host_.checkIgnoreList(ignore_list_, std::bind(&Tray::onRemove, this, std::placeholders::_1));

  item->event_box.signal_show().connect([this] { dp.emit(); });
  item->event_box.signal_hide().connect([this] { dp.emit(); });
  dp.emit();
}

void Tray::onRemove(std::unique_ptr<Item>& item) {
  box_.remove(item->event_box);
  items_.erase(std::remove(items_.begin(), items_.end(), item.get()), items_.end());
  dp.emit();
}

auto Tray::update() -> void {
  // Check if any items should be ignored now that properties have loaded
  if (!ignore_list_.empty()) {
    spdlog::debug("Tray::update() - checking ignore list");
    host_.checkIgnoreList(ignore_list_, std::bind(&Tray::onRemove, this, std::placeholders::_1));
  }

  // Show tray only when items are visible. Iterate the managed items_ list
  // instead of box_.get_children() to avoid a use-after-free on raw widget
  // pointers that may dangle after items are destroyed asynchronously.
  event_box_.set_visible(std::any_of(items_.begin(), items_.end(),
                                     [](Item* item) { return item->event_box.get_visible(); }));
  AModule::update();
}

}  // namespace waybar::modules::SNI
