#include "modules/idle_inhibitor.hpp"

#include "idle-inhibit-unstable-v1-client-protocol.h"
#include <spdlog/spdlog.h>

namespace waybar::modules {

std::list<waybar::AModule*> IdleInhibitor::modules;
bool IdleInhibitor::status = false;

IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar,
                                              const Json::Value& config)
    : ALabel(config, "idle_inhibitor", id, "{status}", 0, false, true),
      bar_(bar),
      idle_inhibitor_(nullptr),
      pid_(-1) {
  if (waybar::Client::inst()->idle_inhibit_manager == nullptr) {
    throw std::runtime_error("idle-inhibit not available");
  }

  if (IdleInhibitor::modules.empty() && config_["start-activated"].isBool() &&
      config_["start-activated"].asBool() != status) {
    toggleStatus();
  }

  // Add this to the modules list
  IdleInhibitor::modules.push_back(this);

  dp.emit();
}

IdleInhibitor::~IdleInhibitor() {
  if (idle_inhibitor_ != nullptr) {
    zwp_idle_inhibitor_v1_destroy(idle_inhibitor_);
    idle_inhibitor_ = nullptr;
  }

  // Remove this from the modules list
  IdleInhibitor::modules.remove(this);

  if (pid_ != -1) {
    kill(-pid_, 9);
    pid_ = -1;
  }
}

auto IdleInhibitor::update() -> void {
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

void IdleInhibitor::toggleStatus() {
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
          for (auto const& module : IdleInhibitor::modules) {
            module->update();
          }
          /* disconnect */
          return false;
        },
        timeoutSecs);
  }
}

void IdleInhibitor::handleToggle(int n_press, double dx, double dy) {
  if (AModule::controllClick_->get_current_button() == 1) {
    toggleStatus();

    // Make all other idle inhibitor modules update
    for (auto const& module : IdleInhibitor::modules) {
      if (module != this) {
        module->update();
      }
    }
  }

  ALabel::handleToggle(n_press, dx, dy);
}

} /* namespace waybar::modules */
