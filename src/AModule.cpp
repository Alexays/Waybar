#include "AModule.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <util/command.hpp>

#include "gdk/gdk.h"
#include "gdkmm/cursor.h"

#include "config.hpp"

namespace waybar {

AModule::AModule(const Json::Value& config, const std::string& name, const std::string& id,
                 bool enable_click, bool enable_scroll)
    : name_(name),
      config_(config),
      isTooltip{config_["tooltip"].isBool() ? config_["tooltip"].asBool() : true},
      isExpand{config_["expand"].isBool() ? config_["expand"].asBool() : false},
      distance_scrolled_y_(0.0),
      distance_scrolled_x_(0.0) {
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

  event_box_.signal_enter_notify_event().connect(sigc::mem_fun(*this, &AModule::handleMouseEnter));
  event_box_.signal_leave_notify_event().connect(sigc::mem_fun(*this, &AModule::handleMouseLeave));

  // configure events' user commands
  // hasUserEvents is true if any element from eventMap_ is satisfying the condition in the lambda
  bool hasUserEvents =
      std::find_if(eventMap_.cbegin(), eventMap_.cend(), [&config](const auto& eventEntry) {
        // True if there is any non-release type event
        return eventEntry.first.second != GdkEventType::GDK_BUTTON_RELEASE &&
               config[eventEntry.second].isString();
      }) != eventMap_.cend();

  if (enable_click || hasUserEvents || config["menu"].isString()) {
    hasUserEvents_ = true;
    event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
    event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &AModule::handleToggle));
  } else {
    hasUserEvents_ = false;
  }

  bool hasReleaseEvent =
      std::find_if(eventMap_.cbegin(), eventMap_.cend(), [&config](const auto& eventEntry) {
        // True if there is any non-release type event
        return eventEntry.first.second == GdkEventType::GDK_BUTTON_RELEASE &&
               config[eventEntry.second].isString();
      }) != eventMap_.cend();
  if (hasReleaseEvent) {
    event_box_.add_events(Gdk::BUTTON_RELEASE_MASK);
    event_box_.signal_button_release_event().connect(sigc::mem_fun(*this, &AModule::handleRelease));
  }
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString() ||
      config_["on-scroll-left"].isString() || config_["on-scroll-right"].isString() ||
      enable_scroll) {
    event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &AModule::handleScroll));
  }

  // Respect user configuration of cursor
  if (config_.isMember("cursor")) {
    if (config_["cursor"].isBool() && config_["cursor"].asBool()) {
      setCursor(Gdk::HAND2);
    } else if (config_["cursor"].isInt()) {
      setCursor(Gdk::CursorType(config_["cursor"].asInt()));
    } else {
      spdlog::warn("unknown cursor option configured on module {}", name_);
    }
  }

  // If a GTKMenu is requested in the config
  if (config_["menu"].isString()) {
    // Create the GTKMenu widget
    try {
      // Check that the file exists
      std::string menuFile = config_["menu-file"].asString();

      // there might be "~" or "$HOME" in original path, try to expand it.
      auto result = Config::tryExpandPath(menuFile, "");
      if (result.empty()) {
        throw std::runtime_error("Failed to expand file: " + menuFile);
      }

      menuFile = result.front();
      // Read the menu descriptor file
      std::ifstream file(menuFile);
      if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + menuFile);
      }
      std::stringstream fileContent;
      fileContent << file.rdbuf();

      // Make the GtkBuilder and check for errors in his parsing
      builder_ = Gtk::Builder::create_from_string(fileContent.str());
      if (!builder_) {
        throw std::runtime_error("Error found in the file " + menuFile);
      }

      builder_->get_widget("menu", menu_);
      if (!menu_) {
        throw std::runtime_error("Failed to get 'menu' object from GtkBuilder");
      }
      auto css_class = id.empty() ? name : id;
      menu_->get_style_context()->add_class(css_class);

      // Linking actions to the GTKMenu based on
      for (Json::Value::const_iterator it = config_["menu-actions"].begin();
           it != config_["menu-actions"].end(); ++it) {
        std::string key = it.key().asString();
        Gtk::MenuItem *item = nullptr;
        builder_->get_widget(key.c_str(), item);
        if (item == nullptr) {
          spdlog::warn("Menu item '{}' not found in builder file", key);
          continue;
        }
        item->get_style_context()->add_class(css_class);
        std::string command = it->asString();
        item->signal_activate().connect(std::function<void()>([this, command] { handleGtkMenuEvent(command); }));
      }

      // Check anchors.
      auto convert_to_gravity = [] (std::string const& anchor) -> std::optional<Gdk::Gravity>
        {
          if (anchor == "north-west")
            return Gdk::Gravity::GRAVITY_NORTH_WEST;
          if (anchor == "north")
            return Gdk::Gravity::GRAVITY_NORTH;
          if (anchor == "north-east")
            return Gdk::Gravity::GRAVITY_NORTH_EAST;
          if (anchor == "west")
            return Gdk::Gravity::GRAVITY_WEST;
          if (anchor == "center")
            return Gdk::Gravity::GRAVITY_CENTER;
          if (anchor == "east")
            return Gdk::Gravity::GRAVITY_EAST;
          if (anchor == "south-west")
            return Gdk::Gravity::GRAVITY_SOUTH_WEST;
          if (anchor == "south")
            return Gdk::Gravity::GRAVITY_SOUTH;
          if (anchor == "south-east")
            return Gdk::Gravity::GRAVITY_SOUTH_EAST;
          return {};
        };

      widget_anchor_ = convert_to_gravity(config_.get("widget-anchor", "none").asString());
      menu_anchor_ = convert_to_gravity(config_.get("menu-anchor", "none").asString());
    } catch (std::runtime_error& e) {
      spdlog::warn("Error while creating the menu : {}. Menu popup not activated.", e.what());
    }
  }
}

AModule::~AModule() {
  for (const auto& pid : pid_children_) {
    if (pid != -1) {
      killpg(pid, SIGTERM);
    }
  }
}

auto AModule::update() -> void {
  // Run user-provided update handler if configured
  if (config_["on-update"].isString()) {
    pid_children_.push_back(util::command::forkExec(config_["on-update"].asString()));
  }
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

void AModule::setCursor(Gdk::CursorType const& c) {
  auto gdk_window = event_box_.get_window();
  if (gdk_window) {
    auto cursor = Gdk::Cursor::create(c);
    gdk_window->set_cursor(cursor);
  } else {
    // window may not be accessible yet, in this case,
    // schedule another call for setting the cursor in 1 sec
    Glib::signal_timeout().connect_seconds(
        [this, c]() {
          setCursor(c);
          return false;
        },
        1);
  }
}

bool AModule::handleMouseEnter(GdkEventCrossing* const& e) {
  if (auto* module = event_box_.get_child(); module != nullptr) {
    module->set_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
  }

  // Default behavior indicating event availability
  if (hasUserEvents_ && !config_.isMember("cursor")) {
    setCursor(Gdk::HAND2);
  }

  return false;
}

bool AModule::handleMouseLeave(GdkEventCrossing* const& e) {
  if (auto* module = event_box_.get_child(); module != nullptr) {
    module->unset_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
  }

  // Default behavior indicating event availability
  if (hasUserEvents_ && !config_.isMember("cursor")) {
    setCursor(Gdk::ARROW);
  }

  return false;
}

bool AModule::handleToggle(GdkEventButton* const& e) { return handleUserEvent(e); }

bool AModule::handleRelease(GdkEventButton* const& e) { return handleUserEvent(e); }

bool AModule::handleUserEvent(GdkEventButton* const& e) {
  std::string format{};
  const std::map<std::pair<uint, GdkEventType>, std::string>::const_iterator& rec{
      eventMap_.find(std::pair(e->button, e->type))};

  if (rec != eventMap_.cend()) {
    // First call module actions
    this->AModule::doAction(rec->second);

    format = rec->second;
  }

  // Check that a menu has been configured
  if (rec != eventMap_.cend() && config_["menu"].isString()) {
    // Check if the event is the one specified for the "menu" option
    if (rec->second == config_["menu"].asString() && menu_ != nullptr) {
      // Popup the menu
      menu_->show_all();
      if (widget_anchor_ && menu_anchor_) {
        menu_->popup_at_widget(&event_box_, *widget_anchor_, *menu_anchor_, reinterpret_cast<GdkEvent*>(e));
      }
      else {
        menu_->popup_at_pointer(reinterpret_cast<GdkEvent*>(e));
      }
      // Manually reset prelight to make sure the module doesn't stay in a hover state
      if (auto* module = event_box_.get_child(); module != nullptr) {
        module->unset_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
      }
    }
  }
  // Second call user scripts
  if (!format.empty()) {
    if (config_[format].isString())
      format = config_[format].asString();
    else
      format.clear();
  }
  if (!format.empty()) {
    pid_children_.push_back(util::command::forkExec(format));
  }
  dp.emit();
  return true;
}

AModule::SCROLL_DIR AModule::getScrollDir(GdkEventScroll* e) {
  // only affects up/down
  bool reverse = config_["reverse-scrolling"].asBool();
  bool reverse_mouse = config_["reverse-mouse-scrolling"].asBool();

  // ignore reverse-scrolling if event comes from a mouse wheel
  GdkDevice* device = gdk_event_get_source_device((GdkEvent*)e);
  if (device != nullptr && gdk_device_get_source(device) == GDK_SOURCE_MOUSE) {
    reverse = reverse_mouse;
  }

  switch (e->direction) {
    case GDK_SCROLL_UP:
      return reverse ? SCROLL_DIR::DOWN : SCROLL_DIR::UP;
    case GDK_SCROLL_DOWN:
      return reverse ? SCROLL_DIR::UP : SCROLL_DIR::DOWN;
    case GDK_SCROLL_LEFT:
      return SCROLL_DIR::LEFT;
    case GDK_SCROLL_RIGHT:
      return SCROLL_DIR::RIGHT;
    case GDK_SCROLL_SMOOTH: {
      SCROLL_DIR dir{SCROLL_DIR::NONE};

      distance_scrolled_y_ += e->delta_y;
      distance_scrolled_x_ += e->delta_x;

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
          distance_scrolled_y_ = 0;
          break;
        case SCROLL_DIR::LEFT:
        case SCROLL_DIR::RIGHT:
          distance_scrolled_x_ = 0;
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

bool AModule::handleScroll(GdkEventScroll* e) {
  auto dir = getScrollDir(e);
  std::string eventName{};

  if (dir == SCROLL_DIR::UP)
    eventName = "on-scroll-up";
  else if (dir == SCROLL_DIR::DOWN)
    eventName = "on-scroll-down";
  else if (dir == SCROLL_DIR::LEFT)
    eventName = "on-scroll-left";
  else if (dir == SCROLL_DIR::RIGHT)
    eventName = "on-scroll-right";

  // First call module actions
  this->AModule::doAction(eventName);
  // Second call user scripts
  if (config_[eventName].isString())
    pid_children_.push_back(util::command::forkExec(config_[eventName].asString()));

  dp.emit();
  return true;
}

void AModule::handleGtkMenuEvent(std::string command) {
  waybar::util::command::forkExec(command, "GtkMenu");
}

bool AModule::tooltipEnabled() const { return isTooltip; }
bool AModule::expandEnabled() const { return isExpand; }

AModule::operator Gtk::Widget&() { return event_box_; }

}  // namespace waybar
