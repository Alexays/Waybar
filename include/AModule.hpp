#pragma once

#include <glibmm/dispatcher.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <json/json.h>

#include "IModule.hpp"

namespace waybar {

class AModule : public IModule {
 public:
  static constexpr const char *MODULE_CLASS = "module";

  virtual ~AModule();
  auto update() -> void override;
  virtual auto refresh(int) -> void{};
  operator Gtk::Widget &() override;
  auto doAction(const std::string &name) -> void override;

  /// Emitting on this dispatcher triggers a update() call
  Glib::Dispatcher dp;

 protected:
  // Don't need to make an object directly
  // Derived classes are able to use it
  AModule(const Json::Value &, const std::string &, const std::string &, bool enable_click = false,
          bool enable_scroll = false);

  enum SCROLL_DIR { NONE, UP, DOWN, LEFT, RIGHT };

  bool tooltipEnabled();

  const std::string name_;
  const Json::Value &config_;
  virtual void handleToggle(int n_press, double dx, double dy);
  virtual void handleRelease(int n_press, double dx, double dy);
  virtual bool handleScroll(double dx, double dy);

 private:
  const bool isTooltip;
  std::vector<int> pid_;
  double distance_scrolled_y_{0.0};
  double distance_scrolled_x_{0.0};
  Glib::RefPtr<Gtk::GestureClick> handleClick_;
  Glib::RefPtr<Gtk::EventControllerScroll> handleScroll_;
  Glib::RefPtr<const Gdk::Event> currEvent_;
  std::map<std::string, std::string> eventActionMap_;
  static const inline std::map<std::pair<std::pair<uint, int>,Gdk::Event::Type>, std::string> eventMap_ {
    {std::make_pair(std::make_pair(1u, 1), Gdk::Event::Type::BUTTON_PRESS), "on-click"},
    {std::make_pair(std::make_pair(1u, 1), Gdk::Event::Type::BUTTON_RELEASE), "on-click-release"},
    {std::make_pair(std::make_pair(1u, 2), Gdk::Event::Type::BUTTON_PRESS), "on-double-click"},
    {std::make_pair(std::make_pair(1u, 3), Gdk::Event::Type::BUTTON_PRESS), "on-triple-click"},
    {std::make_pair(std::make_pair(2u, 1), Gdk::Event::Type::BUTTON_PRESS), "on-click-middle"},
    {std::make_pair(std::make_pair(2u, 1), Gdk::Event::Type::BUTTON_RELEASE), "on-click-middle-release"},
    {std::make_pair(std::make_pair(2u, 2), Gdk::Event::Type::BUTTON_PRESS), "on-double-click-middle"},
    {std::make_pair(std::make_pair(2u, 3), Gdk::Event::Type::BUTTON_PRESS), "on-triple-click-middle"},
    {std::make_pair(std::make_pair(3u, 1), Gdk::Event::Type::BUTTON_PRESS), "on-click-right"},
    {std::make_pair(std::make_pair(3u, 1), Gdk::Event::Type::BUTTON_RELEASE), "on-click-right-release"},
    {std::make_pair(std::make_pair(3u, 2), Gdk::Event::Type::BUTTON_PRESS), "on-double-click-right"},
    {std::make_pair(std::make_pair(3u, 3), Gdk::Event::Type::BUTTON_PRESS), "on-triple-click-right"},
    {std::make_pair(std::make_pair(8u, 1), Gdk::Event::Type::BUTTON_PRESS), "on-click-backward"},
    {std::make_pair(std::make_pair(8u, 1), Gdk::Event::Type::BUTTON_RELEASE), "on-click-backward-release"},
    {std::make_pair(std::make_pair(8u, 2), Gdk::Event::Type::BUTTON_PRESS), "on-double-click-backward"},
    {std::make_pair(std::make_pair(8u, 3), Gdk::Event::Type::BUTTON_PRESS), "on-triple-click-backward"},
    {std::make_pair(std::make_pair(9u, 1), Gdk::Event::Type::BUTTON_PRESS), "on-click-forward"},
    {std::make_pair(std::make_pair(9u, 1), Gdk::Event::Type::BUTTON_RELEASE), "on-click-forward-release"},
    {std::make_pair(std::make_pair(9u, 2), Gdk::Event::Type::BUTTON_PRESS), "on-double-click-forward"},
    {std::make_pair(std::make_pair(9u, 3), Gdk::Event::Type::BUTTON_PRESS), "on-triple-click-forward"}
  };
  void handleClickEvent(uint n_button, int n_press, Gdk::Event::Type n_evtype);
  const SCROLL_DIR getScrollDir(Glib::RefPtr<const Gdk::Event> e);
};

}  // namespace waybar
