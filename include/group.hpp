#pragma once

#include <gtkmm/box.h>
#include <gtkmm/widget.h>
#include <json/json.h>

#include "AModule.hpp"
#include "gtkmm/revealer.h"

namespace waybar {

class Group final : public AModule {
 public:
  Group(const std::string&, const std::string&, const Json::Value&, bool);
  ~Group() override;

  void addWidget(AModule* module);
  Gtk::Box& getBox();

 private:
  auto doUpdate() -> void override;
  void handleMouseEnter(double x, double y) override;
  void handleMouseLeave() override;
  bool handleScroll(double dx, double dy) override;
  void handlePress(int n_press, double x, double y) override;
  void show_group();
  void hide_group();
  void manage_visibility(AModule* module);
  void show_widget(Gtk::Widget& widget);
  void hide_widget(Gtk::Widget& widget);
  void hide_current_widget_if_inactive();

  Gtk::Box m_box_;
  Gtk::Box m_revealer_box_;
  Gtk::Revealer m_revealer_;
  bool m_is_first_widget_{true};
  bool m_is_drawer_{false};
  bool m_click_to_reveal_{false};
  std::string m_always_visible_class_;
  bool m_empty_if_drawer_empty_{false};
  int m_reveal_delay_{0};
  std::string m_add_class_to_drawer_children_;
  sigc::connection m_reveal_timeout_;
};

}  // namespace waybar
