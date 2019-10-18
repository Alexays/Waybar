#pragma once

#include <glibmm/dispatcher.h>
#include <glibmm/markup.h>
#include <gtkmm/eventbox.h>
#include <json/json.h>
#include "IModule.hpp"

namespace waybar {

class AModule : public IModule {
 public:
  AModule(const Json::Value &, const std::string &, const std::string &,
          bool enable_click = false, bool enable_scroll = false);
  virtual ~AModule();
  virtual auto update() -> void;
  virtual      operator Gtk::Widget &();

  Glib::Dispatcher dp;

 protected:
  enum SCROLL_DIR { NONE, UP, DOWN, LEFT, RIGHT };

  SCROLL_DIR getScrollDir(GdkEventScroll *e);
  bool       tooltipEnabled();

  const Json::Value &config_;
  Gtk::EventBox      event_box_;

  virtual bool handleToggle(GdkEventButton *const &ev);
  virtual bool handleScroll(GdkEventScroll *);

 private:
  std::vector<int> pid_;
  gdouble          distance_scrolled_y_;
  gdouble          distance_scrolled_x_;
};

}  // namespace waybar
