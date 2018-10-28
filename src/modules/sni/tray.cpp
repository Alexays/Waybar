#include "modules/sni/tray.hpp"

#include <iostream>

waybar::modules::SNI::Tray::Tray(const Json::Value &config)
    : config_(config), watcher_(), host_(&dp, config)
{
  std::cout << "Tray is in beta, so there may be bugs or even be unusable." << std::endl;
  if (config_["spacing"].isUInt()) {
    box_.set_spacing(config_["spacing"].asUInt());
  }
}

auto waybar::modules::SNI::Tray::update() -> void {
  auto childrens = box_.get_children();
  childrens.erase(childrens.begin(), childrens.end());
  for (auto &item : host_.items) {
    box_.pack_start(item.event_box);
  }
  if (box_.get_children().size() > 0) {
    box_.set_name("tray");
    box_.show_all();
  } else {
    box_.set_name("");
  }
}

waybar::modules::SNI::Tray::operator Gtk::Widget &() {
  return box_;
}
