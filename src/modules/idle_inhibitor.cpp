#include "modules/idle_inhibitor.hpp"

#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "util/command.hpp"

std::list<waybar::AModule*> waybar::modules::IdleInhibitor::modules;
bool waybar::modules::IdleInhibitor::status = false;

waybar::modules::IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar,
                                              const Json::Value& config)
    : ALabel(config, "idle_inhibitor", id, "{status}", 0, false, true),
      bar_(bar),
      idle_inhibitor_(nullptr),
      pid_(-1),
      wait_for_activity_(false) {
  if (waybar::Client::inst()->idle_inhibit_manager == nullptr) {
    throw std::runtime_error("idle-inhibit not available");
  }

  // Read the wait-for-activity config option
  if (config_["wait-for-activity"].isBool()) {
    wait_for_activity_ = config_["wait-for-activity"].asBool();
  }

  if (waybar::modules::IdleInhibitor::modules.empty() && config_["start-activated"].isBool() &&
      config_["start-activated"].asBool() != status) {
    toggleStatus();
  }

  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &IdleInhibitor::handleToggle));

  // Add this to the modules list
  waybar::modules::IdleInhibitor::modules.push_back(this);

  dp.emit();
}

waybar::modules::IdleInhibitor::~IdleInhibitor() {
  teardownActivityMonitoring();

  if (idle_inhibitor_ != nullptr) {
    zwp_idle_inhibitor_v1_destroy(idle_inhibitor_);
    idle_inhibitor_ = nullptr;
  }

  // Remove this from the modules list
  waybar::modules::IdleInhibitor::modules.remove(this);

  if (pid_ != -1) {
    kill(-pid_, 9);
    pid_ = -1;
  }
}

auto waybar::modules::IdleInhibitor::update() -> void {
  // Check status
  if (status) {
    label_.get_style_context()->remove_class("deactivated");
    if (idle_inhibitor_ == nullptr) {
      idle_inhibitor_ = zwp_idle_inhibit_manager_v1_create_inhibitor(
          waybar::Client::inst()->idle_inhibit_manager, bar_.surface);
    }
  } else {
    label_.get_style_context()->remove_class("activated");
    if (idle_inhibitor_ != nullptr) {
      zwp_idle_inhibitor_v1_destroy(idle_inhibitor_);
      idle_inhibitor_ = nullptr;
    }
  }

  std::string status_text = status ? "activated" : "deactivated";
  label_.set_markup(fmt::format(fmt::runtime(format_), fmt::arg("status", status_text),
                                fmt::arg("icon", getIcon(0, status_text))));
  label_.get_style_context()->add_class(status_text);
  if (tooltipEnabled()) {
    auto config = config_[status ? "tooltip-format-activated" : "tooltip-format-deactivated"];
    auto tooltip_format = config.isString() ? config.asString() : "{status}";
    label_.set_tooltip_markup(fmt::format(fmt::runtime(tooltip_format),
                                          fmt::arg("status", status_text),
                                          fmt::arg("icon", getIcon(0, status_text))));
  }
  // Call parent update
  ALabel::update();
}

auto waybar::modules::IdleInhibitor::refresh(int sig) -> void {
  if (config_["signal"].isInt() && sig == SIGRTMIN + config_["signal"].asInt()) {
    toggleStatus();

    // Make all other idle inhibitor modules update
    for (auto const& module : waybar::modules::IdleInhibitor::modules) {
      module->update();
    }
  }
}

void waybar::modules::IdleInhibitor::toggleStatus() {
  status = !status;

  if (timeout_.connected()) {
    /* cancel any already active timeout handler */
    timeout_.disconnect();
  }

  if (status && config_["timeout"].isNumeric()) {
    auto timeoutMins = config_["timeout"].asDouble();
    int timeoutSecs = timeoutMins * 60;

    // If wait-for-activity is enabled, set up activity monitoring
    if (wait_for_activity_) {
      setupActivityMonitoring();
      resetActivityTimeout();
    } else {
      // Original behavior: simple timeout
      timeout_ = Glib::signal_timeout().connect_seconds(
          []() {
            /* intentionally not tied to a module instance lifetime
             * as the output with `this` can be disconnected
             */
            spdlog::info("deactivating idle_inhibitor by timeout");
            status = false;
            for (auto const& module : waybar::modules::IdleInhibitor::modules) {
              module->update();
            }
            /* disconnect */
            return false;
          },
          timeoutSecs);
    }
  } else {
    // When deactivated, tear down activity monitoring
    teardownActivityMonitoring();
  }
}

bool waybar::modules::IdleInhibitor::handleToggle(GdkEventButton* const& e) {
  if (e->button == 1) {
    toggleStatus();

    // Make all other idle inhibitor modules update
    for (auto const& module : waybar::modules::IdleInhibitor::modules) {
      if (module != this) {
        module->update();
      }
    }
  }

  ALabel::handleToggle(e);
  return true;
}

bool waybar::modules::IdleInhibitor::handleMotion(GdkEventMotion* const& e) {
  if (wait_for_activity_ && status) {
    resetActivityTimeout();
  }
  return false;
}

bool waybar::modules::IdleInhibitor::handleKey(GdkEventKey* const& e) {
  if (wait_for_activity_ && status) {
    resetActivityTimeout();
  }
  return false;
}

void waybar::modules::IdleInhibitor::resetActivityTimeout() {
  if (!config_["timeout"].isNumeric()) {
    return;
  }

  if (activity_timeout_.connected()) {
    activity_timeout_.disconnect();
  }

  auto timeoutMins = config_["timeout"].asDouble();
  int timeoutSecs = timeoutMins * 60;

  activity_timeout_ = Glib::signal_timeout().connect_seconds(
      []() {
        spdlog::info("deactivating idle_inhibitor due to inactivity");
        status = false;
        for (auto const& module : waybar::modules::IdleInhibitor::modules) {
          module->update();
        }
        return false;
      },
      timeoutSecs);
}

void waybar::modules::IdleInhibitor::setupActivityMonitoring() {
  // Don't set up if already connected
  if (motion_connection_.connected() || key_connection_.connected()) {
    return;
  }

  // Enable motion and key event monitoring on the bar window
  auto window = bar_.window.get_window();
  if (window) {
    window->set_events(window->get_events() | Gdk::POINTER_MOTION_MASK | Gdk::KEY_PRESS_MASK);
  }

  // Connect to the bar window's event signals
  motion_connection_ = bar_.window.signal_motion_notify_event().connect(
      sigc::mem_fun(*this, &IdleInhibitor::handleMotion));
  key_connection_ = bar_.window.signal_key_press_event().connect(
      sigc::mem_fun(*this, &IdleInhibitor::handleKey));
}

void waybar::modules::IdleInhibitor::teardownActivityMonitoring() {
  if (activity_timeout_.connected()) {
    activity_timeout_.disconnect();
  }
  
  if (motion_connection_.connected()) {
    motion_connection_.disconnect();
  }
  
  if (key_connection_.connected()) {
    key_connection_.disconnect();
  }
}
