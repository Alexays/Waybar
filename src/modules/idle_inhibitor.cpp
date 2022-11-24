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
      pid_(-1) {
  if (waybar::Client::inst()->idle_inhibit_manager == nullptr) {
    throw std::runtime_error("idle-inhibit not available");
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
  label_.set_markup(fmt::format(format_, fmt::arg("status", status_text),
                                fmt::arg("icon", getIcon(0, status_text))));
  label_.get_style_context()->add_class(status_text);
  if (tooltipEnabled()) {
    label_.set_tooltip_markup(
        status ? fmt::format(config_["tooltip-format-activated"].isString()
                                 ? config_["tooltip-format-activated"].asString()
                                 : "{status}",
                             fmt::arg("status", status_text),
                             fmt::arg("icon", getIcon(0, status_text)))
               : fmt::format(config_["tooltip-format-deactivated"].isString()
                                 ? config_["tooltip-format-deactivated"].asString()
                                 : "{status}",
                             fmt::arg("status", status_text),
                             fmt::arg("icon", getIcon(0, status_text))));
  }
  // Call parent update
  ALabel::update();
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
