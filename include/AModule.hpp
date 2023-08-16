#pragma once

#include <glibmm/dispatcher.h>
#include <glibmm/markup.h>
#include <gtkmm/eventbox.h>
#include <json/json.h>

#include "IModule.hpp"

namespace waybar {

class AModule : public IModule {
 public:
  virtual ~AModule();
  auto update() -> void override;
  virtual auto refresh(int) -> void{};
  operator Gtk::Widget &() override;
  auto doAction(const std::string &name) -> void override;

  Glib::Dispatcher dp;

 protected:
  // Don't need to make an object directly
  // Derived classes are able to use it
  AModule(const Json::Value &, const std::string &, const std::string &, bool enable_click = false,
          bool enable_scroll = false);

  enum SCROLL_DIR { NONE, UP, DOWN, LEFT, RIGHT };

  SCROLL_DIR getScrollDir(GdkEventScroll *e);
  bool tooltipEnabled();

  const std::string name_;
  const Json::Value &config_;
  Gtk::EventBox event_box_;

  virtual bool handleToggle(GdkEventButton *const &ev);
  virtual bool handleScroll(GdkEventScroll *);

 private:
  std::vector<int> pid_;
  gdouble distance_scrolled_y_;
  gdouble distance_scrolled_x_;
  std::map<std::string, std::string> eventActionMap_;
  static const inline std::map<std::pair<uint, GdkEventType>, std::string> eventMap_{
      {std::make_pair(1, GdkEventType::GDK_BUTTON_PRESS), "on-click"},
      {std::make_pair(1, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click"},
      {std::make_pair(1, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click"},
      {std::make_pair(2, GdkEventType::GDK_BUTTON_PRESS), "on-click-middle"},
      {std::make_pair(2, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click-middle"},
      {std::make_pair(2, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click-middle"},
      {std::make_pair(3, GdkEventType::GDK_BUTTON_PRESS), "on-click-right"},
      {std::make_pair(3, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click-right"},
      {std::make_pair(3, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click-right"},
      {std::make_pair(8, GdkEventType::GDK_BUTTON_PRESS), "on-click-backward"},
      {std::make_pair(8, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click-backward"},
      {std::make_pair(8, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click-backward"},
      {std::make_pair(9, GdkEventType::GDK_BUTTON_PRESS), "on-click-forward"},
      {std::make_pair(9, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click-forward"},
      {std::make_pair(9, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click-forward"}};
};

}  // namespace waybar
