#include "modules/sni/tray.hpp"

#include <iostream>

waybar::modules::SNI::Tray::Tray(const Json::Value& config)
  : config_(config), watcher_(), host_(dp)
{
}

auto waybar::modules::SNI::Tray::update() -> void
{
  for (auto item : host_.items) {
    item.image->set_tooltip_text(item.title);
    box_.pack_start(*item.image);
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
