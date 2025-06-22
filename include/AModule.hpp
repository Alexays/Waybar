#pragma once

#include <glibmm/dispatcher.h>
#include <glibmm/markup.h>
#include <gtkmm.h>
#include <gtkmm/eventbox.h>
#include <json/json.h>

#include "IModule.hpp"

namespace waybar {

class AModule : public IModule {
 public:
  static constexpr const char *MODULE_CLASS = "module";

  ~AModule() override;
  auto update() -> void override;
  virtual auto refresh(int shouldRefresh) -> void {};
  operator Gtk::Widget &() override;
  auto doAction(const std::string &name) -> void override;

  /// Emitting on this dispatcher triggers a update() call
  Glib::Dispatcher dp;

  bool expandEnabled() const;

 protected:
  // Don't need to make an object directly
  // Derived classes are able to use it
  AModule(const Json::Value &, const std::string &, const std::string &, bool enable_click = false,
          bool enable_scroll = false);

  enum SCROLL_DIR { NONE, UP, DOWN, LEFT, RIGHT };

  SCROLL_DIR getScrollDir(GdkEventScroll *e);
  bool tooltipEnabled() const;

  std::vector<int> pid_children_;
  const std::string name_;
  const Json::Value &config_;
  Gtk::EventBox event_box_;

  virtual void setCursor(Gdk::CursorType const &c);

  virtual bool handleToggle(GdkEventButton *const &ev);
  virtual bool handleMouseEnter(GdkEventCrossing *const &ev);
  virtual bool handleMouseLeave(GdkEventCrossing *const &ev);
  virtual bool handleScroll(GdkEventScroll *);
  virtual bool handleRelease(GdkEventButton *const &ev);
  GObject *menu_;

 private:
  bool handleUserEvent(GdkEventButton *const &ev);
  const bool isTooltip;
  const bool isExpand;
  bool hasUserEvents_;
  gdouble distance_scrolled_y_;
  gdouble distance_scrolled_x_;
  std::map<std::string, std::string> eventActionMap_;
  static const inline std::map<std::pair<uint, GdkEventType>, std::string> eventMap_{
      {std::make_pair(1, GdkEventType::GDK_BUTTON_PRESS), "on-click"},
      {std::make_pair(1, GdkEventType::GDK_BUTTON_RELEASE), "on-click-release"},
      {std::make_pair(1, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click"},
      {std::make_pair(1, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click"},
      {std::make_pair(2, GdkEventType::GDK_BUTTON_PRESS), "on-click-middle"},
      {std::make_pair(2, GdkEventType::GDK_BUTTON_RELEASE), "on-click-middle-release"},
      {std::make_pair(2, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click-middle"},
      {std::make_pair(2, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click-middle"},
      {std::make_pair(3, GdkEventType::GDK_BUTTON_PRESS), "on-click-right"},
      {std::make_pair(3, GdkEventType::GDK_BUTTON_RELEASE), "on-click-right-release"},
      {std::make_pair(3, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click-right"},
      {std::make_pair(3, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click-right"},
      {std::make_pair(8, GdkEventType::GDK_BUTTON_PRESS), "on-click-backward"},
      {std::make_pair(8, GdkEventType::GDK_BUTTON_RELEASE), "on-click-backward-release"},
      {std::make_pair(8, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click-backward"},
      {std::make_pair(8, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click-backward"},
      {std::make_pair(9, GdkEventType::GDK_BUTTON_PRESS), "on-click-forward"},
      {std::make_pair(9, GdkEventType::GDK_BUTTON_RELEASE), "on-click-forward-release"},
      {std::make_pair(9, GdkEventType::GDK_2BUTTON_PRESS), "on-double-click-forward"},
      {std::make_pair(9, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click-forward"}};
};

}  // namespace waybar
