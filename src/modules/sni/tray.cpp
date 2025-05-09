#include "modules/sni/tray.hpp"

#include <spdlog/spdlog.h>

#include "modules/sni/icon_manager.hpp"

namespace waybar::modules::SNI {

Tray::Tray(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "tray", id),
      box_(bar.orientation, 0),
      watcher_(SNI::Watcher::getInstance()),
      host_(nb_hosts_, config, bar, std::bind(&Tray::onAdd, this, std::placeholders::_1),
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
  nb_hosts_ += 1;
  if (config_["icons"].isObject()) {
    IconManager::instance().setIconsConfig(config_["icons"]);
  }
  dp.emit();
}

void Tray::onAdd(std::unique_ptr<Item>& item) {
  if (config_["reverse-direction"].isBool() && config_["reverse-direction"].asBool()) {
    box_.pack_end(item->event_box);
  } else {
    box_.pack_start(item->event_box);
  }
  dp.emit();
}

void Tray::onRemove(std::unique_ptr<Item>& item) {
  box_.remove(item->event_box);
  dp.emit();
}

auto Tray::update() -> void {
  // Show tray only when items are available
  event_box_.set_visible(!box_.get_children().empty());
  // Call parent update
  AModule::update();
}

}  // namespace waybar::modules::SNI
