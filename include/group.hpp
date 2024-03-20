#pragma once

#include <gtkmm/box.h>
#include <gtkmm/widget.h>
#include <json/json.h>

#include "AModule.hpp"
#include "gtkmm/revealer.h"

namespace waybar {

class Group : public AModule {
 public:
  Group(const std::string&, const std::string&, const Json::Value&, bool);
  virtual ~Group() = default;
  auto update() -> void override;

  virtual Gtk::Box& getBox();
  void addWidget(Gtk::Widget& widget);

  bool handleMouseHover(GdkEventCrossing* const& e);

 protected:
  Gtk::Box box;
  Gtk::Box revealer_box;
  Gtk::Revealer revealer;
  bool is_first_widget = true;
  bool is_drawer = false;
  std::string add_class_to_drawer_children;

  void addHoverHandlerTo(Gtk::Widget& widget);
};

}  // namespace waybar
