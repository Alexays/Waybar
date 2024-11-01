#pragma once

#include <gtkmm/box.h>
#include <gtkmm/revealer.h>

#include "AModule.hpp"

namespace waybar {

class Group final : public AModule {
 public:
  Group(const std::string &, const std::string &, const Json::Value &, bool);
  ~Group() override = default;
  auto update() -> void override;

  virtual Gtk::Box &getBox();
  void addWidget(Gtk::Widget &widget);

  Gtk::Widget &root() override;

 private:
  Gtk::Box box_;
  Gtk::Box revealer_box_;
  Gtk::Revealer revealer_;
  bool is_first_widget_ = true;
  bool is_drawer_ = false;
  bool click_to_reveal_ = false;
  std::string add_class_to_drawer_children_;

  void handleMouseEnter(double x, double y) override;
  void handleMouseLeave() override;
  void handleToggle(int n_press, double dx, double dy) override;
  void show_group();
  void hide_group();
};

}  // namespace waybar
