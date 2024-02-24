#pragma once

#include "AModule.hpp"

#include <gtkmm/box.h>
#include <gtkmm/eventcontrollermotion.h>
#include "gtkmm/revealer.h"

namespace waybar {

class Group : public AModule {
 public:
  Group(const std::string&, const std::string&, const Json::Value&, bool);
  virtual ~Group() = default;
  auto update() -> void override;
  operator Gtk::Widget&() override;

  virtual Gtk::Box& getBox();
  void addWidget(Gtk::Widget& widget);

 protected:
  Gtk::Box box;
  Gtk::Box revealer_box;
  Gtk::Revealer revealer;
  bool is_first_widget{true};
  bool is_drawer{false};
  std::string add_class_to_drawer_children;

 private:
  Glib::RefPtr<Gtk::EventControllerMotion> controllMotion_;
  void addHoverHandlerTo(Gtk::Widget& widget);
  void onMotionEnter(double x, double y);
  void onMotionLeave();
};

}  // namespace waybar
