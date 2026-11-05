#pragma once

#include <fmt/args.h>
#include <fmt/format.h>
#include <glibmm/markup.h>
#include <gtkmm.h>
#include <json/json.h>
#include <sigc++/scoped_connection.h>

#include <array>
#include <string>
#include <utility>

#include "IModule.hpp"
#include "util/coalescing_dispatcher.hpp"
#include "util/module_box.hpp"

namespace waybar {

// A pointer button event, in the coordinates of the module widget. A double (triple) click is
// reported as a PRESS followed by a DOUBLE (TRIPLE), like GTK3 did.
struct ButtonEvent {
  enum class Type : uint8_t { PRESS, DOUBLE, TRIPLE, RELEASE };
  guint button;
  Type type;
  double x;
  double y;
};

struct ScrollEvent {
  double dx;
  double dy;
  // GDK_SOURCE_MOUSE for a wheel, which reverse-mouse-scrolling applies to.
  GdkInputSource source;
};

class AModule : public IModule {
 public:
  static constexpr const char* MODULE_CLASS = "module";

  ~AModule() override;
  sigc::signal<void(AModule*)> signal_updated;
  auto update() -> void override;
  virtual auto refresh(int shouldRefresh) -> void {};
  operator Gtk::Widget&() override;
  auto doAction(const std::string& name) -> void override;

  /// Emitting on this dispatcher triggers a update() call. It starts pending: Bar runs the first
  /// update from the main loop, so emits before it never write to the dispatcher pipe.
  util::CoalescingDispatcher dp{true};

  bool expandEnabled() const;

  virtual void suspend() {};
  virtual void resume() {};
  bool shouldSuspend() const { return disable_on_sleep_; }

 protected:
  // Don't need to make an object directly
  // Derived classes are able to use it
  AModule(const Json::Value&, const std::string&, const std::string&, bool enable_click = false,
          bool enable_scroll = false);

  enum SCROLL_DIR { NONE, UP, DOWN, LEFT, RIGHT };

  SCROLL_DIR getScrollDir(const ScrollEvent& e);
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
    widget.set_tooltip_markup(
        fmt::format(fmt::runtime(resolveTooltipFormat(defaultFormat)), std::forward<Args>(args)...));
  }

  std::vector<int> pid_children_;
  const std::string name_;
  const Json::Value& config_;
  util::ModuleBox event_box_;

  // Event controllers are only attached on demand: a module that listens to no click or scroll
  // must let the event reach its parent (e.g. a group). Both are idempotent.
  void enableClick();
  void enableScroll();
  // Moves the scroll controller to the bar window, so that scrolling anywhere on the bar reaches
  // handleScroll (enable-bar-scroll).
  void enableBarScroll(Gtk::Widget& bar_window);
  // GTK3 let double and triple presses through child buttons that took the single ones. Modules
  // whose buttons did so call this, for on-double-click and on-triple-click to see them.
  void enableChildMultiClick();

  // Return true to stop the event from reaching the parent widgets.
  virtual bool handleToggle(const ButtonEvent& ev);
  virtual bool handleRelease(const ButtonEvent& ev);
  virtual bool handleScroll(const ScrollEvent& ev);

  // Builds menu_ from the `menu-file` (a GTK3 GtkBuilder GtkMenu) and `menu-actions` options.
  void setupMenu();

  bool disable_on_sleep_{false};
  Gtk::PopoverMenu* menu_ = nullptr;

  // Maps a configured event name (e.g. "on-click-middle") to a built-in module
  // action name. Populated from the `actions` config section, and by modules
  // that interpret on-click* config values as internal actions (e.g.
  // wlr/taskbar). Entries here are dispatched through doAction() instead of
  // being run as shell commands.
  std::map<std::string, std::string> eventActionMap_;

 private:
  bool handleUserEvent(const ButtonEvent& ev);
  void onPressed(int n_press, double x, double y);
  void onReleased(int n_press, double x, double y);
  bool onCapturedEvent(const Glib::RefPtr<const Gdk::Event>& event);
  bool onScroll(double dx, double dy);
  void setCursor(const std::string& name);

  const bool isTooltip;
  const bool isExpand;
  bool hasUserEvents_;
  bool hasReleaseEvent_;
  double distance_scrolled_y_ = 0.0;
  double distance_scrolled_x_ = 0.0;
  Glib::RefPtr<Gtk::GestureClick> click_;
  Glib::RefPtr<Gtk::EventControllerScroll> scroll_;
  // The last two presses, counted like GDK3 counted them, and the multiple press waiting to reach
  // handleToggle unless click_ sees the press
  struct Press {
    guint32 time;
    GtkWidget* window;
    guint button;
    double x, y;
  };
  std::array<Press, 2> presses_{};
  sigc::scoped_connection multi_press_;
  Gtk::Widget* scroll_target_ = nullptr;
  using Type = ButtonEvent::Type;
  static const inline std::map<std::pair<uint, Type>, std::string> eventMap_{
      {std::make_pair(1, Type::PRESS), "on-click"},
      {std::make_pair(1, Type::RELEASE), "on-click-release"},
      {std::make_pair(1, Type::DOUBLE), "on-double-click"},
      {std::make_pair(1, Type::TRIPLE), "on-triple-click"},
      {std::make_pair(2, Type::PRESS), "on-click-middle"},
      {std::make_pair(2, Type::RELEASE), "on-click-middle-release"},
      {std::make_pair(2, Type::DOUBLE), "on-double-click-middle"},
      {std::make_pair(2, Type::TRIPLE), "on-triple-click-middle"},
      {std::make_pair(3, Type::PRESS), "on-click-right"},
      {std::make_pair(3, Type::RELEASE), "on-click-right-release"},
      {std::make_pair(3, Type::DOUBLE), "on-double-click-right"},
      {std::make_pair(3, Type::TRIPLE), "on-triple-click-right"},
      {std::make_pair(8, Type::PRESS), "on-click-backward"},
      {std::make_pair(8, Type::RELEASE), "on-click-backward-release"},
      {std::make_pair(8, Type::DOUBLE), "on-double-click-backward"},
      {std::make_pair(8, Type::TRIPLE), "on-triple-click-backward"},
      {std::make_pair(9, Type::PRESS), "on-click-forward"},
      {std::make_pair(9, Type::RELEASE), "on-click-forward-release"},
      {std::make_pair(9, Type::DOUBLE), "on-double-click-forward"},
      {std::make_pair(9, Type::TRIPLE), "on-triple-click-forward"},
      {std::make_pair(10, Type::PRESS), "on-click-copy"}};
};

}  // namespace waybar
