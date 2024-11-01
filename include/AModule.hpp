#pragma once

#include <glibmm/dispatcher.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gestureclick.h>
#include <json/json.h>

#include "IModule.hpp"

namespace waybar {

// Representation of a specific type of mouse event
struct MouseEvent {
  // Number of button pressed
  uint n_button;
  // Number of times the button was pressed
  int n_press;
  // Whether it was a press or release
  Gdk::Event::Type evt_type;

  bool operator<(const MouseEvent o) const {
    return std::tie(n_button, n_press, evt_type) < std::tie(o.n_button, o.n_press, o.evt_type);
  }
};

class AModule : public IModule {
 public:
  static constexpr const char *MODULE_CLASS = "module";

  ~AModule() override;
  auto update() -> void override;
  virtual auto refresh(int shouldRefresh) -> void {};
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

  const SCROLL_DIR getScrollDir(Glib::RefPtr<const Gdk::Event> e);
  bool tooltipEnabled() const;

  const std::string name_;
  const Json::Value &config_;
  Glib::RefPtr<Gtk::GestureClick> controllClick_;
  Glib::RefPtr<Gtk::EventControllerScroll> controllScroll_;
  Glib::RefPtr<Gtk::EventControllerMotion> controllMotion_;

  void bindEvents(Gtk::Widget &wg);
  void unBindEvents();

  virtual void setCursor(const Glib::RefPtr<Gdk::Cursor> &cur);
  virtual void setCursor(const Glib::ustring &name);

  virtual void handleToggle(int n_press, double dx, double dy);
  virtual void handleRelease(int n_press, double dx, double dy);
  virtual bool handleScroll(double dx, double dy);
  virtual void handleMouseEnter(double x, double y);
  virtual void handleMouseLeave();

 private:
  const bool isTooltip;
  const bool isExpand;
  const bool isAfter{true};
  bool enableClick_{false};
  bool enableScroll_{false};
  bool hasPressEvents_{false};
  bool hasReleaseEvents_{false};
  std::vector<int> pid_;
  gdouble distance_scrolled_y_{0.0};
  gdouble distance_scrolled_x_{0.0};
  const Glib::RefPtr<Gdk::Cursor> curDefault;
  const Glib::RefPtr<Gdk::Cursor> curPoint;
  Glib::RefPtr<const Gdk::Event> currEvent_;
  std::map<std::string, std::string> eventActionMap_;
  static const inline std::map<MouseEvent, std::string> eventMap_{
      {{1u, 1, Gdk::Event::Type::BUTTON_PRESS}, "on-click"},
      {{1u, 1, Gdk::Event::Type::BUTTON_RELEASE}, "on-click-release"},
      {{1u, 2, Gdk::Event::Type::BUTTON_PRESS}, "on-double-click"},
      {{1u, 3, Gdk::Event::Type::BUTTON_PRESS}, "on-triple-click"},
      {{2u, 1, Gdk::Event::Type::BUTTON_PRESS}, "on-click-middle"},
      {{2u, 1, Gdk::Event::Type::BUTTON_RELEASE}, "on-click-middle-release"},
      {{2u, 2, Gdk::Event::Type::BUTTON_PRESS}, "on-double-click-middle"},
      {{2u, 3, Gdk::Event::Type::BUTTON_PRESS}, "on-triple-click-middle"},
      {{3u, 1, Gdk::Event::Type::BUTTON_PRESS}, "on-click-right"},
      {{3u, 1, Gdk::Event::Type::BUTTON_RELEASE}, "on-click-right-release"},
      {{3u, 2, Gdk::Event::Type::BUTTON_PRESS}, "on-double-click-right"},
      {{3u, 3, Gdk::Event::Type::BUTTON_PRESS}, "on-triple-click-right"},
      {{8u, 1, Gdk::Event::Type::BUTTON_PRESS}, "on-click-backward"},
      {{8u, 1, Gdk::Event::Type::BUTTON_RELEASE}, "on-click-backward-release"},
      {{8u, 2, Gdk::Event::Type::BUTTON_PRESS}, "on-double-click-backward"},
      {{8u, 3, Gdk::Event::Type::BUTTON_PRESS}, "on-triple-click-backward"},
      {{9u, 1, Gdk::Event::Type::BUTTON_PRESS}, "on-click-forward"},
      {{9u, 1, Gdk::Event::Type::BUTTON_RELEASE}, "on-click-forward-release"},
      {{9u, 2, Gdk::Event::Type::BUTTON_PRESS}, "on-double-click-forward"},
      {{9u, 3, Gdk::Event::Type::BUTTON_PRESS}, "on-triple-click-forward"}};
  void handleClickEvent(uint n_button, int n_press, Gdk::Event::Type n_evtype);
  void makeControllClick();
  void makeControllScroll();
  void makeControllMotion();
  void removeControllClick();
  void removeControllScroll();
  void removeControllMotion();
};

}  // namespace waybar
