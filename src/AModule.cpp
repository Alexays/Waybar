#include "AModule.hpp"

#include <fmt/core.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <util/command.hpp>

#include "gdk/gdk.h"
#include "gdkmm/cursor.h"

namespace waybar {

AModule::AModule(const Json::Value& config, const std::string& name, const std::string& id,
                 bool enable_click, bool enable_scroll)
    : name_(name),
      config_(config),
      enableClick_{enable_click},
      enableScroll_{enable_scroll},
      isTooltip_{config_["tooltip"].isBool() ? config_["tooltip"].asBool() : true},
      isExpand_{config_["expand"].isBool() ? config_["expand"].asBool() : false},
      distance_scrolled_y_(0.0),
      distance_scrolled_x_(0.0),
      curDefault_{Gdk::Cursor::create("default")},
      curPoint_{Gdk::Cursor::create("pointer")},
      cursor_timeout_conn_() {
  // Configure module action Map
  const Json::Value actions{config_["actions"]};

  disable_on_sleep_ =
      config_["disable-on-sleep"].isBool() ? config_["disable-on-sleep"].asBool() : false;

  for (Json::Value::const_iterator it = actions.begin(); it != actions.end(); ++it) {
    if (it.key().isString() && it->isString())
      if (!eventActionMap_.contains(it.key().asString())) {
        eventActionMap_.insert({it.key().asString(), it->asString()});
        enable_click = true;
        enable_scroll = true;
      } else
        spdlog::warn("Duplicate action is ignored: {0}", it.key().asString());
    else
      spdlog::warn("Wrong actions section configuration. See config by index: {}", it.index());
  }

  // configure events' user commands
  // hasUserEvents is true if any element from eventMap_ is satisfying the condition in the lambda
  bool hasPressEvents{
      std::find_if(eventMap_.cbegin(), eventMap_.cend(), [&config](const auto& eventEntry) {
        // True if there is any non-release type event
        const auto& [key, action] = eventEntry;
        const auto& [button, n_press, ev_type] = key;
        return ev_type != Gdk::Event::Type::BUTTON_RELEASE && config[action].isString();
      }) != eventMap_.cend()};
  if (enable_click || hasPressEvents) {
    hasPressEvents_ = true;
  } else {
    hasPressEvents_ = false;
  }

  hasReleaseEvents_ =
      std::find_if(eventMap_.cbegin(), eventMap_.cend(), [&config](const auto& eventEntry) {
        // True if there is any release type event
        const auto& [key, action] = eventEntry;
        const auto& [button, n_press, ev_type] = key;
        return ev_type == Gdk::Event::Type::BUTTON_RELEASE && config[action].isString();
      }) != eventMap_.cend();

  makeGestureClick();
  makeControllerScroll();
  makeControllerMotion();

  // Respect user configuration of cursor
  if (config_.isMember("cursor")) {
    if (config_["cursor"].isBool() && config_["cursor"].asBool()) {
      setCursor(curPoint_);
    } else if (config_["cursor"].isString()) {
      setCursor(config_["cursor"].asString());
    } else {
      spdlog::warn("unknown cursor option configured on module {}", name_);
    }
  }
}

AModule::~AModule() {
  if (cursor_timeout_conn_.connected()) {
    cursor_timeout_conn_.disconnect();
  }
  for (const auto& pid : pid_children_) {
    if (pid != -1) {
      killpg(pid, SIGTERM);
    }
  }
}

auto AModule::doUpdate() -> void {
  // Run user-provided update handler if configured
  if (config_["on-update"].isString()) {
    pid_children_.push_back(util::command::forkExec(config_["on-update"].asString()));
  }
  m_signal_updated_.emit(this);
}
// Get mapping between event name and module action name
// Then call overridden doAction in order to call appropriate module action
auto AModule::doAction(const std::string& name) -> void {
  if (!name.empty()) {
    const std::map<std::string, std::string>::const_iterator& recA{eventActionMap_.find(name)};
    // Call overridden action if derived class has implemented it
    if (recA != eventActionMap_.cend() && name != recA->second) this->doAction(recA->second);
  }
}

void AModule::setCursor(const Glib::ustring& name) {
  this->operator Gtk::Widget&().set_cursor(name);
}

void AModule::setCursor(const Glib::RefPtr<Gdk::Cursor>& cur) {
  this->operator Gtk::Widget&().set_cursor(cur);
}

void AModule::handleMouseEnter(double x, double y) {
  this->operator Gtk::Widget&().set_state_flags(Gtk::StateFlags::PRELIGHT);

  // Default behavior indicating event availability
  if (hasPressEvents_ && !config_.isMember("cursor")) {
    setCursor(curPoint_);
  }
}

void AModule::handleMouseLeave() {
  this->operator Gtk::Widget&().unset_state_flags(Gtk::StateFlags::PRELIGHT);

  // Default behavior indicating event availability
  if (hasPressEvents_ && !config_.isMember("cursor")) {
    setCursor(curDefault_);
  }
}

void AModule::handlePress(int n_press, double x, double y) {
  handleClickEvent(gesture_click_->get_current_event()->get_button(), n_press, x, y,
                   Gdk::Event::Type::BUTTON_PRESS);
}

void AModule::handleRelease(int n_press, double x, double y) {
  handleClickEvent(gesture_click_->get_current_event()->get_button(), n_press, x, y,
                   Gdk::Event::Type::BUTTON_RELEASE);
}

void AModule::handleClickEvent(uint n_button, int n_press, double x, double y,
                               Gdk::Event::Type n_evtype) {
  std::string format{};
  const std::map<std::tuple<uint, int, Gdk::Event::Type>, std::string>::const_iterator& rec{
      eventMap_.find(std::tuple(n_button, n_press, n_evtype))};
  if (rec != eventMap_.cend()) {
    // First call module action
    this->AModule::doAction(rec->second);
    format = rec->second;
  }
  // Second call user scripts
  if (!format.empty()) {
    const auto actionIt{eventActionMap_.find(format)};
    const bool isModuleAction{actionIt != eventActionMap_.cend() && config_[format].isString() &&
                              config_[format].asString() == actionIt->second};

    if (isModuleAction || !config_[format].isString())
      format.clear();
    else
      format = config_[format].asString();
  }
  if (!format.empty()) {
    std::string cmd{format};
    const auto width{w_->get_width()};
    const auto height{w_->get_height()};

    if (format.find("{x}") != std::string::npos || format.find("{y}") != std::string::npos) {
      try {
        cmd = fmt::format(fmt::runtime(format), fmt::arg("x", (int)round(100. * x / width)),
                          fmt::arg("y", (int)round(100. * y / height)));
      } catch (const fmt::format_error& err) {
        spdlog::warn("Failed to format command '{}': {}. Running it unformatted.", format,
                     err.what());
        cmd = format;
      }
    }
    pid_children_.push_back(util::command::forkExec(format));
  }

  dp.emit();
}

const AModule::SCROLL_DIR AModule::getScrollDir(Glib::RefPtr<const Gdk::Event> e) {
  // only affects up/down
  bool reverse{config_["reverse-scrolling"].asBool()};
  bool reverse_mouse{config_["reverse-mouse-scrolling"].asBool()};

  // ignore reverse-scrolling if event comes from a mouse wheel
  const auto device{e->get_device()};
  if ((device) && device->get_source() == Gdk::InputSource::MOUSE) reverse = reverse_mouse;

  switch (e->get_direction()) {
    case Gdk::ScrollDirection::UP:
      return reverse ? SCROLL_DIR::DOWN : SCROLL_DIR::UP;
    case Gdk::ScrollDirection::DOWN:
      return reverse ? SCROLL_DIR::UP : SCROLL_DIR::DOWN;
    case Gdk::ScrollDirection::LEFT:
      return reverse ? SCROLL_DIR::RIGHT : SCROLL_DIR::LEFT;
    case Gdk::ScrollDirection::RIGHT:
      return reverse ? SCROLL_DIR::LEFT : SCROLL_DIR::RIGHT;
    case Gdk::ScrollDirection::SMOOTH: {
      SCROLL_DIR dir{SCROLL_DIR::NONE};

      double delta_x, delta_y;
      e->get_deltas(delta_x, delta_y);

      distance_scrolled_y_ += delta_y;
      distance_scrolled_x_ += delta_x;

      double threshold{0.0};
      if (config_["smooth-scrolling-threshold"].isNumeric()) {
        threshold = config_["smooth-scrolling-threshold"].asDouble();
      }

      if (distance_scrolled_y_ < -threshold) {
        dir = reverse ? SCROLL_DIR::DOWN : SCROLL_DIR::UP;
      } else if (distance_scrolled_y_ > threshold) {
        dir = reverse ? SCROLL_DIR::UP : SCROLL_DIR::DOWN;
      } else if (distance_scrolled_x_ > threshold) {
        dir = SCROLL_DIR::RIGHT;
      } else if (distance_scrolled_x_ < -threshold) {
        dir = SCROLL_DIR::LEFT;
      }

      switch (dir) {
        case SCROLL_DIR::UP:
        case SCROLL_DIR::DOWN:
          distance_scrolled_y_ = 0.0;
          break;
        case SCROLL_DIR::LEFT:
        case SCROLL_DIR::RIGHT:
          distance_scrolled_x_ = 0.0;
          break;
        case SCROLL_DIR::NONE:
          break;
      }

      return dir;
    }
    // Silence -Wreturn-type:
    default:
      return SCROLL_DIR::NONE;
  }
}

bool AModule::handleScroll(double dx, double dy) {
  currEvent_ = controller_scroll_->get_current_event();

  if (currEvent_) {
    std::string format{};
    const auto dir{getScrollDir(currEvent_)};

    if (dir == SCROLL_DIR::UP)
      format = "on-scroll-up";
    else if (dir == SCROLL_DIR::DOWN)
      format = "on-scroll-down";
    else if (dir == SCROLL_DIR::LEFT)
      format = "on-scroll-left";
    else if (dir == SCROLL_DIR::RIGHT)
      format = "on-scroll-right";

    // First call module action
    this->AModule::doAction(format);
    // Second call user scripts
    if (config_[format].isString())
      pid_children_.push_back(util::command::forkExec(config_[format].asString()));

    dp.emit();
  }

  return true;
}

bool AModule::tooltipEnabled() const { return isTooltip_; }
bool AModule::expandEnabled() const { return isExpand_; }

void AModule::makeGestureClick() {
  if (enableClick_ || hasPressEvents_ || hasReleaseEvents_) {
    gesture_click_ = Gtk::GestureClick::create();
    gesture_click_->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    gesture_click_->set_button(0u);

    if (enableClick_ || hasPressEvents_)
      gesture_click_->signal_pressed().connect(sigc::mem_fun(*this, &AModule::handlePress),
                                               isAfter_);
    if (hasReleaseEvents_)
      gesture_click_->signal_released().connect(sigc::mem_fun(*this, &AModule::handleRelease),
                                                isAfter_);
  }
}

void AModule::makeControllerScroll() {
  if (enableScroll_ || config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString() ||
      config_["on-scroll-left"].isString() || config_["on-scroll-right"].isString()) {
    controller_scroll_ = Gtk::EventControllerScroll::create();
    controller_scroll_->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    controller_scroll_->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
    controller_scroll_->signal_scroll().connect(sigc::mem_fun(*this, &AModule::handleScroll),
                                                isAfter_);
  }
}

void AModule::makeControllerMotion() {
  controller_motion_ = Gtk::EventControllerMotion::create();
  controller_motion_->signal_enter().connect(sigc::mem_fun(*this, &AModule::handleMouseEnter));
  controller_motion_->signal_leave().connect(sigc::mem_fun(*this, &AModule::handleMouseLeave));
}

static void removeController(Glib::RefPtr<Gtk::EventController> controller) {
  if (controller) {
    Gtk::Widget* widget{controller->get_widget()};
    if (widget) widget->remove_controller(controller);
  }
}

void AModule::removeGestureClick() {
  if (gesture_click_) {
    removeController(gesture_click_);
    gesture_click_ = nullptr;
  }
}

void AModule::removeControllerScroll() {
  if (controller_scroll_) {
    removeController(controller_scroll_);
    controller_scroll_ = nullptr;
  }
}

void AModule::removeControllerMotion() {
  if (controller_motion_) {
    removeController(controller_motion_);
    controller_motion_ = nullptr;
  }
}

void AModule::bindEvents(Gtk::Widget& wg) {
  wg.set_cursor(curDefault_);

  if (!gesture_click_) makeGestureClick();
  if (!controller_scroll_) makeControllerScroll();
  if (!controller_motion_) makeControllerMotion();

  if (gesture_click_) wg.add_controller(gesture_click_);
  if (controller_scroll_) wg.add_controller(controller_scroll_);
  if (controller_motion_) wg.add_controller(controller_motion_);
}

void AModule::unBindEvents() {
  removeGestureClick();
  removeControllerScroll();
  removeControllerMotion();
}

}  // namespace waybar
