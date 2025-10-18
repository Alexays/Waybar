#include "modules/idle_inhibitor.hpp"

#include "ext-idle-notify-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "util/command.hpp"

std::list<waybar::AModule*> waybar::modules::IdleInhibitor::modules;
bool waybar::modules::IdleInhibitor::status = false;

waybar::modules::IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar,
                                              const Json::Value& config)
    : ALabel(config, "idle_inhibitor", id, "{status}", 0, false, true),
      bar_(bar),
      idle_inhibitor_(nullptr),
      idle_notification_(nullptr),
      idle_timeout_ms_(0),
      pid_(-1),
      wait_for_activity_(false) {
  if (waybar::Client::inst()->idle_inhibit_manager == nullptr) {
    throw std::runtime_error("idle-inhibit not available");
  }

  // Read the wait-for-activity config option
  if (config_["wait-for-activity"].isBool()) {
    wait_for_activity_ = config_["wait-for-activity"].asBool();
    
    // Check if ext-idle-notify protocol is available when wait-for-activity is enabled
    if (wait_for_activity_ && waybar::Client::inst()->idle_notifier == nullptr) {
      throw std::runtime_error("wait-for-activity requires ext-idle-notify-v1 protocol support");
    }
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
  teardownIdleNotification();

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
    idle_timeout_ms_ = timeoutSecs * 1000;

    // If wait-for-activity is enabled, set up idle notification
    if (wait_for_activity_) {
      setupIdleNotification();
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
    // When deactivated, tear down idle notification
    teardownIdleNotification();
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

void waybar::modules::IdleInhibitor::handleIdled(void* data, 
                                                  struct ::ext_idle_notification_v1* /*notification*/) {
  spdlog::info("deactivating idle_inhibitor due to user inactivity");
  status = false;
  for (auto const& module : waybar::modules::IdleInhibitor::modules) {
    module->update();
  }
}

void waybar::modules::IdleInhibitor::handleResumed(void* data,
                                                    struct ::ext_idle_notification_v1* /*notification*/) {
  // User became active again - notification will continue monitoring
  spdlog::debug("user activity detected, idle_inhibitor still active");
}

void waybar::modules::IdleInhibitor::setupIdleNotification() {
  // Don't set up if already exists
  if (idle_notification_ != nullptr) {
    return;
  }

  auto* client = waybar::Client::inst();
  if (client->idle_notifier == nullptr) {
    spdlog::error("ext-idle-notify protocol not available");
    return;
  }

  // Get the wayland seat from the display
  auto* gdk_seat = gdk_display_get_default_seat(client->gdk_display->gobj());
  if (gdk_seat == nullptr) {
    spdlog::error("failed to get default seat");
    return;
  }
  auto* wl_seat = gdk_wayland_seat_get_wl_seat(gdk_seat);

  // Create idle notification that monitors all input (not just when inhibitor is active)
  // We use get_idle_notification instead of get_input_idle_notification to respect
  // idle inhibitors from other applications
  idle_notification_ = ext_idle_notifier_v1_get_idle_notification(
      client->idle_notifier, idle_timeout_ms_, wl_seat);

  static const struct ext_idle_notification_v1_listener idle_notification_listener = {
      .idled = &IdleInhibitor::handleIdled,
      .resumed = &IdleInhibitor::handleResumed,
  };

  ext_idle_notification_v1_add_listener(idle_notification_, &idle_notification_listener, this);
  wl_display_roundtrip(client->wl_display);
}

void waybar::modules::IdleInhibitor::teardownIdleNotification() {
  if (idle_notification_ != nullptr) {
    ext_idle_notification_v1_destroy(idle_notification_);
    idle_notification_ = nullptr;
  }
}
