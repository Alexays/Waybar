#pragma once

#include <fmt/args.h>
#include <fmt/format.h>
#include <glibmm/dispatcher.h>
#include <glibmm/markup.h>
#include <gtkmm.h>
#include <gtkmm/eventbox.h>
#include <json/json.h>

#include <string>
#include <utility>

#include "IModule.hpp"

namespace waybar {

class AModule : public IModule {
 public:
  static constexpr const char* MODULE_CLASS = "module";

  ~AModule() override;
  auto update() -> void override;
  virtual auto refresh(int shouldRefresh) -> void {};
  operator Gtk::Widget&() override;
  auto doAction(const std::string& name) -> void override;

  /// Emitting on this dispatcher triggers a update() call
  Glib::Dispatcher dp;

  bool expandEnabled() const;

  std::mutex& reap_mtx;
  std::list<pid_t>& reap;

  virtual void suspend() {};
  virtual void resume() {};
  bool shouldSuspend() const { return disable_on_sleep_; }

 protected:
  // Don't need to make an object directly
  // Derived classes are able to use it
  AModule(const Json::Value&, const std::string&, const std::string&, std::mutex& reap_mtx,
          std::list<pid_t>& reap, bool enable_click = false, bool enable_scroll = false);

  enum SCROLL_DIR { NONE, UP, DOWN, LEFT, RIGHT };

  SCROLL_DIR getScrollDir(GdkEventScroll* e);
  bool tooltipEnabled() const;

  // --- Generic format/tooltip resolution (config-only, usable by any module,
  // ALabel-derived or not). Prefers `<key>-<state>`, then `<key>`, then default.
  std::string resolveFormat(const std::string& defaultFormat, const std::string& state = "") const {
    if (!state.empty() && config_["format-" + state].isString()) {
      return config_["format-" + state].asString();
    }
    if (config_["format"].isString()) {
      return config_["format"].asString();
    }
    return defaultFormat;
  }
  std::string resolveTooltipFormat(const std::string& defaultFormat,
                                   const std::string& state = "") const {
    if (!state.empty() && config_["tooltip-format-" + state].isString()) {
      return config_["tooltip-format-" + state].asString();
    }
    if (config_["tooltip-format"].isString()) {
      return config_["tooltip-format"].asString();
    }
    return defaultFormat;
  }

  // Generic tooltip for any widget: honors the `tooltip` toggle and
  // `tooltip-format`, formats with the given args and applies it. Lets modules
  // that are not ALabel-derived (e.g. gamemode) reuse the shared logic.
  template <typename... Args>
  void updateTooltip(Gtk::Widget& widget, const std::string& defaultFormat, Args&&... args) {
    if (!tooltipEnabled()) {
      return;
    }
    widget.set_tooltip_markup(fmt::format(fmt::runtime(resolveTooltipFormat(defaultFormat)),
                                          std::forward<Args>(args)...));
  }

  std::vector<int> pid_children_;
  const std::string name_;
  const Json::Value& config_;
  Gtk::EventBox event_box_;

  virtual void setCursor(std::string const& c);

  virtual bool handleToggle(GdkEventButton* const& ev);
  virtual bool handleMouseEnter(GdkEventCrossing* const& ev);
  virtual bool handleMouseLeave(GdkEventCrossing* const& ev);
  virtual bool handleScroll(GdkEventScroll*);
  virtual bool handleRelease(GdkEventButton* const& ev);

  bool disable_on_sleep_{false};
  GObject* menu_ = nullptr;

 private:
  bool handleUserEvent(GdkEventButton* const& ev);
  const bool isTooltip;
  const bool isExpand;
  bool hasUserEvents_;
  gdouble distance_scrolled_y_;
  gdouble distance_scrolled_x_;
  sigc::connection cursor_timeout_conn_;
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
      {std::make_pair(9, GdkEventType::GDK_3BUTTON_PRESS), "on-triple-click-forward"},
      {std::make_pair(10, GdkEventType::GDK_BUTTON_PRESS), "on-click-copy"}};
};

}  // namespace waybar
