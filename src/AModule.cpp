#include "AModule.hpp"

#include <util/command.hpp>
namespace waybar {

AModule::AModule(const Json::Value& config, const std::string& name, const std::string& id,
                 bool enable_click, bool enable_scroll)
    : name_(std::move(name)),
      config_(std::move(config)),
      isTooltip{config_["tooltip"].isBool() ? config_["tooltip"].asBool() : true},
      enableClick_{enable_click},
      enableScroll_{enable_scroll} {
  // Configure module action Map
  const Json::Value actions{config_["actions"]};
  for (Json::Value::const_iterator it{actions.begin()}; it != actions.end(); ++it) {
    if (it.key().isString() && it->isString())
      if (eventActionMap_.count(it.key().asString()) == 0) {
        eventActionMap_.insert({it.key().asString(), it->asString()});
        enable_click = true;
        enable_scroll = true;
      } else
        spdlog::warn("Dublicate action is ignored: {0}", it.key().asString());
    else
      spdlog::warn("Wrong actions section configuration. See config by index: {}", it.index());
  }

  // configure events' user commands
  // hasUserEvent is true if any element from eventMap_ is satisfying the condition in the lambda

  hasUsrPressEvent_ = std::find_if(eventMap_.cbegin(), eventMap_.cend(), [&config](const auto& eventEntry) {
    // True if there is any non-release type event
    return eventEntry.first.second == Gdk::Event::Type::BUTTON_PRESS &&
           config[eventEntry.second].isString();
  }) != eventMap_.cend();

  hasUsrReleaseEvent_ = std::find_if(eventMap_.cbegin(), eventMap_.cend(), [&config](const auto& eventEntry) {
    // True if there is any release type event
    return eventEntry.first.second == Gdk::Event::Type::BUTTON_RELEASE &&
           config[eventEntry.second].isString();
  }) != eventMap_.cend();

  makeControllClick();
  makeControllScroll();
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
  handleClickEvent(controllClick_->get_current_button(), n_press, Gdk::Event::Type::BUTTON_PRESS);
}
void AModule::handleRelease(int n_press, double dx, double dy) {
  handleClickEvent(controllClick_->get_current_button(), n_press, Gdk::Event::Type::BUTTON_RELEASE);
}

void AModule::handleClickEvent(uint n_button, int n_press, Gdk::Event::Type n_evtype) {
  std::string format{};
  const std::map<std::pair<std::pair<uint, int>, Gdk::Event::Type>, std::string>::const_iterator& rec {
    eventMap_.find(std::pair(std::pair(n_button, n_press), n_evtype))};
  if (rec != eventMap_.cend()) {
    // First call module action
    this->AModule::doAction(rec->second);
    format = rec->second;
  }
  // Second call user scripts
  if (!format.empty()) {
    if (config_[format].isString())
      format = config_[format].asString();
    else
      format.clear();
  }
  if (!format.empty())
    pid_.push_back(util::command::forkExec(format));

  dp.emit();
}

const AModule::SCROLL_DIR AModule::getScrollDir(Glib::RefPtr<const Gdk::Event> e) {
  // only affects up/down
  bool reverse{config_["reverse-scrolling"].asBool()};
  bool reverse_mouse{config_["reverse-mouse-scrolling"].asBool()};

  // ignore reverse-scrolling if event comes from a mouse wheel
  const auto device{e->get_device()};
  if (device->get_source() == Gdk::InputSource::MOUSE)
    reverse = reverse_mouse;

  switch(e->get_direction()) {
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

      gdouble threshold{0.0};
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

    if (dir == SCROLL_DIR::UP || dir == SCROLL_DIR::RIGHT)
      format = "on-scroll-up";
    else if (dir == SCROLL_DIR::DOWN || dir == SCROLL_DIR::LEFT)
      format = "on-scroll-down";

    // First call module action
    this->AModule::doAction(format);
    // Second call user scripts
    if (config_[format].isString())
      pid_.push_back(util::command::forkExec(config_[format].asString()));

    dp.emit();
  }

  return true;
}

bool AModule::tooltipEnabled() { return isTooltip; }

AModule::operator Gtk::Widget&() { return this->operator Gtk::Widget&(); };

void AModule::bindEvents(Gtk::Widget& wg) {
  if (!controllClick_) makeControllClick();
  if (!controllScroll_) makeControllScroll();

  if (controllClick_) wg.add_controller(controllClick_);
  if (controllScroll_) wg.add_controller(controllScroll_);
}

void AModule::unBindEvents() {
  removeControllClick();
  removeControllScroll();
}

void AModule::makeControllClick() {
  if (enableClick_ || hasUsrPressEvent_ || hasUsrReleaseEvent_) {
    controllClick_ = Gtk::GestureClick::create();
    controllClick_->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    controllClick_->set_button(0u);

    if (enableClick_ || hasUsrPressEvent_)
      controllClick_->signal_pressed().connect(sigc::mem_fun(*this, &AModule::handleToggle), isAfter);
    if (hasUsrReleaseEvent_)
      controllClick_->signal_released().connect(sigc::mem_fun(*this, &AModule::handleRelease), isAfter);
  }
}

void AModule::makeControllScroll() {
  if (enableScroll_ || config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    controllScroll_ = Gtk::EventControllerScroll::create();
    controllScroll_->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    controllScroll_->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
    controllScroll_->signal_scroll().connect(sigc::mem_fun(*this, &AModule::handleScroll), isAfter);
  }
}

static void removeControll(Glib::RefPtr<Gtk::EventController> controll) {
  if (controll) {
    Gtk::Widget *widget{controll->get_widget()};
    if (widget)
      widget->remove_controller(controll);
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

}  // namespace waybar
