#pragma once

#include <gtkmm/box.h>
#include <gtkmm/widget.h>
#include <json/json.h>

#include "AModule.hpp"
#include "gtkmm/revealer.h"

#include "util/sleeper_thread.hpp"

namespace waybar {

class Group : public AModule {
 public:
  Group(const std::string &, const std::string &, const Json::Value &, bool);
  ~Group() override = default;
  auto update() -> void override;
  operator Gtk::Widget &() override;

  virtual Gtk::Box &getBox();
  void addWidget(Gtk::Widget &widget);

 protected:
  Gtk::Box box;
  Gtk::Box revealer_box;
  Gtk::Revealer revealer;
  bool is_first_widget = true;
  bool is_drawer = false;
  bool click_to_reveal = false;
  std::string add_class_to_drawer_children;
  bool handleMouseEnter(GdkEventCrossing *const &ev) override;
  bool handleMouseLeave(GdkEventCrossing *const &ev) override;
  bool handleToggle(GdkEventButton *const &ev) override;
  void refresh(int sig) override;
  void show_group();
  void hide_group();
  util::SleeperThread thread_;
  bool reset_ = false;
  bool free_ = true;
  std::chrono::seconds interval_;

};

}  // namespace waybar
