#pragma once

#include <gtkmm/box.h>
#include <gtkmm/revealer.h>
#include <gtkmm/widget.h>
#include <json/json.h>

#include "AModule.hpp"

namespace waybar {

class Group : public AModule {
  sigc::connection reveal_timeout_;

 public:
  Group(const std::string&, const std::string&, const Json::Value&, bool);
  ~Group() override;
  auto update() -> void override;
  operator Gtk::Widget&() override;

  virtual Gtk::Box& getBox();
  void addWidget(AModule* module);

 protected:
  Gtk::Box box;
  Gtk::Box revealer_box;
  Gtk::Box leader_revealer_box;
  Gtk::Revealer revealer;
  Gtk::Revealer leader_revealer;
  bool is_first_widget = true;
  bool is_drawer = false;
  bool click_to_reveal = false;
  bool hide_leader_when_expanded = false;
  bool drawer_left_to_right = true;
  std::string always_visible_class;
  bool empty_if_drawer_empty = false;
  int reveal_delay = 0;
  std::string add_class_to_drawer_children;
  // Children of `box` packed at its start, in order. The others are packed at its end, like
  // gtk_box_pack_end() did: each one before the previous ones.
  std::vector<Gtk::Widget*> box_start_;
  Glib::RefPtr<Gtk::EventControllerMotion> motion_;
  void packStart(Gtk::Widget& widget);
  void packEnd(Gtk::Widget& widget);
  void removeFromBox(Gtk::Widget& widget);
  void handleMouseEnter();
  void handleMouseLeave();
  bool handleToggle(const ButtonEvent& ev) override;
  bool handleScroll(const ScrollEvent& e) override;
  void show_group();
  void hide_group();
  void manage_visibility(AModule* module);
  void show_widget(Gtk::Widget& widget);
  void hide_widget(Gtk::Widget& widget);
  void hide_current_widget_if_inactive();
};

}  // namespace waybar
