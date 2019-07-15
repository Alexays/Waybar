#include "modules/sni/tray.hpp"
#include <spdlog/spdlog.h>

namespace waybar::modules::SNI {

Tray::Tray(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "tray", id),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
      watcher_(nb_hosts_),
      host_(nb_hosts_, config, std::bind(&Tray::onAdd, this, std::placeholders::_1),
            std::bind(&Tray::onRemove, this, std::placeholders::_1)) {
  spdlog::warn(
      "For a functional tray you must have libappindicator-* installed and export "
      "XDG_CURRENT_DESKTOP=Unity");
  box_.set_name("tray");
  event_box_.add(box_);
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  if (config_["spacing"].isUInt()) {
    box_.set_spacing(config_["spacing"].asUInt());
  }
  nb_hosts_ += 1;
  dp.emit();
}

void Tray::onAdd(std::unique_ptr<Item>& item) {
  box_.pack_start(item->event_box);
  dp.emit();
}

void Tray::onRemove(std::unique_ptr<Item>& item) {
  box_.remove(item->event_box);
  dp.emit();
}

auto Tray::update() -> void {
  if (box_.get_children().empty()) {
    box_.hide();
  } else {
    box_.show_all();
  }
}

}  // namespace waybar::modules::SNI
