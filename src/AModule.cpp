#include "AModule.hpp"

#include <util/command.hpp>
namespace waybar {

AModule::AModule(const Json::Value& config, const std::string& name, const std::string& id,
                 bool enable_click, bool enable_scroll)
    : name_(name),
      config_(config),
      isTooltip{config_["tooltip"].isBool() ? config_["tooltip"].asBool() : true},
      isExpand{config_["expand"].isBool() ? config_["expand"].asBool() : false},
      enableClick_{enable_click},
      enableScroll_{enable_scroll},
      curDefault{Gdk::Cursor::create("default")},
      curPoint{Gdk::Cursor::create("pointer")} {
  // Configure module action Map
  const Json::Value actions{config_["actions"]};

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
  bool hasPressEvents =
      std::find_if(eventMap_.cbegin(), eventMap_.cend(), [&config](const auto& eventEntry) {
        // True if there is any non-release type event
        return eventEntry.first.evt_type != Gdk::Event::Type::BUTTON_RELEASE &&
               config[eventEntry.second].isString();
      }) != eventMap_.cend();
  if (enable_click || hasPressEvents) {
    hasPressEvents_ = true;
  } else {
    hasPressEvents_ = false;
  }

  hasReleaseEvents_ =
      std::find_if(eventMap_.cbegin(), eventMap_.cend(), [&config](const auto& eventEntry) {
        // True if there is any release type event
        return eventEntry.first.evt_type == Gdk::Event::Type::BUTTON_RELEASE &&
               config[eventEntry.second].isString();
      }) != eventMap_.cend();

  makeControllClick();
  makeControllScroll();
  makeControllMotion();

  // Respect user configuration of cursor
  if (config_.isMember("cursor")) {
    if (config_["cursor"].isBool() && config_["cursor"].asBool()) {
      setCursor(curPoint);
    } else if (config_["cursor"].isString()) {
      setCursor(config_["cursor"].asString());
    } else {
      spdlog::warn("unknown cursor option configured on module {}", name_);
    }
  }
}

AModule::~AModule() {
  for (const auto& pid : pid_) {
    if (pid != -1) {
      killpg(pid, SIGTERM);
    }
  }
}

auto AModule::update() -> void {
  // Run user-provided update handler if configured
  if (config_["on-update"].isString()) {
    pid_.push_back(util::command::forkExec(config_["on-update"].asString()));
  }
}
// Get mapping between event name and module action name
// Then call overrided doAction in order to call appropriate module action
auto AModule::doAction(const std::string& name) -> void {
  if (!name.empty()) {
    const std::map<std::string, std::string>::const_iterator& recA{eventActionMap_.find(name)};
    // Call overrided action if derrived class has implemented it
    if (recA != eventActionMap_.cend() && name != recA->second) this->doAction(recA->second);
  }
}

void AModule::handleToggle(int n_press, double dx, double dy) {
  handleRawClickEvent(controllClick_->get_current_button(), n_press,
                      Gdk::Event::Type::BUTTON_PRESS);
}
void AModule::handleRelease(int n_press, double dx, double dy) {
  handleRawClickEvent(controllClick_->get_current_button(), n_press,
                      Gdk::Event::Type::BUTTON_RELEASE);
}

void AModule::setCursor(const Glib::RefPtr<Gdk::Cursor>& cur) { root().set_cursor(cur); }

void AModule::setCursor(const Glib::ustring& name) { root().set_cursor(name); }

void AModule::handleMouseEnter(double x, double y) {
  controllMotion_->get_widget()->set_state_flags(Gtk::StateFlags::PRELIGHT);

  // Default behavior indicating event availability
  if (hasPressEvents_ && !config_.isMember("cursor")) {
    setCursor(curPoint);
  }
}

void AModule::handleMouseLeave() {
  controllMotion_->get_widget()->unset_state_flags(Gtk::StateFlags::PRELIGHT);

  // Default behavior indicating event availability
  if (hasPressEvents_ && !config_.isMember("cursor")) {
    setCursor(curDefault);
  }
}

void AModule::handleRawClickEvent(uint n_button, int n_press, Gdk::Event::Type n_evtype) {
  std::string format{};
  const std::map<MouseEvent, std::string>::const_iterator& rec =
      eventMap_.find(MouseEvent{n_button, n_press, n_evtype});
  if (rec != eventMap_.cend()) {
    // First call module action
    this->AModule::doAction(rec->second);
    format = rec->second;

    // Second allow subclass to handle the event by name
    handleClick(rec->second);
  }

  // Finally call user scripts
  if (!format.empty()) {
    if (config_[format].isString())
      format = config_[format].asString();
    else
      format.clear();
  }
  if (!format.empty()) pid_.push_back(util::command::forkExec(format));

  dp.emit();
}

void AModule::handleClick(const std::string& name) {}

const AModule::SCROLL_DIR AModule::getScrollDir(Glib::RefPtr<const Gdk::Event> e) {
  // only affects up/down
  bool reverse = config_["reverse-scrolling"].asBool();
  bool reverse_mouse = config_["reverse-mouse-scrolling"].asBool();

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

      gdouble threshold = 0;
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
  currEvent_ = controllScroll_->get_current_event();

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
      pid_.push_back(util::command::forkExec(config_[format].asString()));

    dp.emit();
  }

  return true;
}

bool AModule::tooltipEnabled() const { return isTooltip; }
bool AModule::expandEnabled() const { return isExpand; }

void AModule::bindEvents(Gtk::Widget& wg) {
  wg.set_cursor(curDefault);

  if (!controllClick_) makeControllClick();
  if (!controllScroll_) makeControllScroll();
  if (!controllMotion_) makeControllMotion();

  if (controllClick_) wg.add_controller(controllClick_);
  if (controllScroll_) wg.add_controller(controllScroll_);
  if (controllMotion_) wg.add_controller(controllMotion_);
}

void AModule::unBindEvents() {
  removeControllClick();
  removeControllScroll();
  removeControllMotion();
}

void AModule::makeControllClick() {
  if (enableClick_ || hasPressEvents_ || hasReleaseEvents_) {
    controllClick_ = Gtk::GestureClick::create();
    controllClick_->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    controllClick_->set_button(0u);

    if (enableClick_ || hasPressEvents_)
      controllClick_->signal_pressed().connect(sigc::mem_fun(*this, &AModule::handleToggle),
                                               isAfter);
    if (hasReleaseEvents_)
      controllClick_->signal_released().connect(sigc::mem_fun(*this, &AModule::handleRelease),
                                                isAfter);
  }
}

void AModule::makeControllScroll() {
  if (enableScroll_ || config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString() ||
      config_["on-scroll-left"].isString() || config_["on-scroll-right"].isString()) {
    controllScroll_ = Gtk::EventControllerScroll::create();
    controllScroll_->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    controllScroll_->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
    controllScroll_->signal_scroll().connect(sigc::mem_fun(*this, &AModule::handleScroll), isAfter);
  }
}

void AModule::makeControllMotion() {
  controllMotion_ = Gtk::EventControllerMotion::create();
  controllMotion_->signal_enter().connect(sigc::mem_fun(*this, &AModule::handleMouseEnter));
  controllMotion_->signal_leave().connect(sigc::mem_fun(*this, &AModule::handleMouseLeave));
}

static void removeControll(Glib::RefPtr<Gtk::EventController> controll) {
  if (controll) {
    Gtk::Widget* widget{controll->get_widget()};
    if (widget) widget->remove_controller(controll);
  }
}

void AModule::removeControllClick() {
  if (controllClick_) {
    removeControll(controllClick_);
    controllClick_ = nullptr;
  }
}

void AModule::removeControllScroll() {
  if (controllScroll_) {
    removeControll(controllScroll_);
    controllScroll_ = nullptr;
  }
}

void AModule::removeControllMotion() {
  if (controllMotion_) {
    removeControll(controllMotion_);
    controllMotion_ = nullptr;
  }
}

}  // namespace waybar
