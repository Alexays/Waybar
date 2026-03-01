#include "modules/sni/tray.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

#include "modules/sni/icon_manager.hpp"

namespace waybar::modules::SNI {

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
      host_(nb_hosts_, config, bar, ignore_list_,
            std::bind(&Tray::onAdd, this, std::placeholders::_1),
            std::bind(&Tray::onRemove, this, std::placeholders::_1)) {
  box_.set_name("tray");
  event_box_.add(box_);
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  if (config_["spacing"].isUInt()) {
    box_.set_spacing(config_["spacing"].asUInt());
  }
  if (config["show-passive-items"].isBool()) {
    show_passive_ = config["show-passive-items"].asBool();
  }
  nb_hosts_ += 1;
  if (config_["icons"].isObject()) {
    IconManager::instance().setIconsConfig(config_["icons"]);
  }
  dp.emit();
}

void Tray::checkIgnoreList(std::unique_ptr<Item>* item_ptr) {
  // Delegate to Host's checkIgnoreList method
  host_.checkIgnoreList(ignore_list_, std::bind(&Tray::onRemove, this, std::placeholders::_1));
}

void Tray::onAdd(std::unique_ptr<Item>& item) {
  spdlog::info("Tray::onAdd - item bus_name='{}', category='{}', icon_name='{}', title='{}'", 
              item->bus_name, item->category, item->icon_name, item->title);
  
  if (config_["reverse-direction"].isBool() && config_["reverse-direction"].asBool()) {
    box_.pack_end(item->event_box);
  } else {
    box_.pack_start(item->event_box);
  }

  spdlog::debug("Tray::onAdd deferred check - checking ignore list");
  host_.checkIgnoreList(ignore_list_, std::bind(&Tray::onRemove, this, std::placeholders::_1));
  
  dp.emit();
}

void Tray::onRemove(std::unique_ptr<Item>& item) {
  box_.remove(item->event_box);
  dp.emit();
}

auto Tray::update() -> void {
  // Check if any items should be ignored now that properties have loaded
  if (!ignore_list_.empty()) {
    spdlog::debug("Tray::update() - checking ignore list");
    host_.checkIgnoreList(ignore_list_, std::bind(&Tray::onRemove, this, std::placeholders::_1));
  }
  
  // Show tray only when items are available
  std::vector<Gtk::Widget*> children = box_.get_children();
  if (show_passive_) {
    event_box_.set_visible(!children.empty());
  } else {
    event_box_.set_visible(!std::all_of(children.begin(), children.end(), [](Gtk::Widget* child) {
      return child->get_style_context()->has_class("passive");
    }));
  }

  // Call parent update
  AModule::update();
}

}  // namespace waybar::modules::SNI
