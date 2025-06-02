#include "modules/idle_inhibitor.hpp"

#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "util/command.hpp"

std::list<waybar::AModule*> waybar::modules::IdleInhibitor::modules;
bool waybar::modules::IdleInhibitor::status = false;
long waybar::modules::IdleInhibitor::deactivationTime = time(nullptr);

waybar::modules::IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar,
                                              const Json::Value& config)
    : ALabel(config, "idle_inhibitor", id, "{status}", 0, false, true),
      bar_(bar),
      idle_inhibitor_(nullptr),
      pid_(-1),
      timeout(config_["timeout"].asDouble()),
      timeout_step(config_["timeout-step"].isDouble() ? config_["timeout-step"].asDouble() : 10) {
  if (waybar::Client::inst()->idle_inhibit_manager == nullptr) {
    throw std::runtime_error("idle-inhibit not available");
  }

  if (waybar::modules::IdleInhibitor::modules.empty() && config_["start-activated"].isBool() &&
      config_["start-activated"].asBool() != status) {
    toggleStatus();
  }

  deactivationTime = time(nullptr) + timeout * 60;

  event_box_.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  event_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &IdleInhibitor::handleToggle));

  event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &IdleInhibitor::handleScroll));

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
  int timeleft = (deactivationTime - time(nullptr)) / 60;
  label_.set_markup(fmt::format(fmt::runtime(format_), fmt::arg("status", status_text),
                                fmt::arg("timeout", timeout), fmt::arg("timeleft", timeleft),
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

void waybar::modules::IdleInhibitor::toggleStatus(int force_status) {
  status = !status;
  if (force_status != -1) {
    status = force_status;
  }

  if (timeout_.connected()) {
    /* cancel any already active timeout handler */
    timeout_.disconnect();
  }

  if (status && timeout) {
    deactivationTime = time(nullptr) + timeout * 60;
    timeout_ = Glib::signal_timeout().connect_seconds(
        []() {
          /* intentionally not tied to a module instance lifetime
           * as the output with `this` can be disconnected
           */
          bool continueRunning = true;
          int timeleft = (deactivationTime - time(nullptr)) / 60;
          spdlog::info("updating timeleft. deactivation timestamp: {}, minutes left: {}",
                       deactivationTime, timeleft);
          if (timeleft <= 0) {
            spdlog::info("deactivating idle_inhibitor by timeout");
            status = false;
            continueRunning = false;
          }
          for (auto const& module : waybar::modules::IdleInhibitor::modules) {
            module->update();
          }
          return continueRunning;
        },
        60);
  }
}

bool waybar::modules::IdleInhibitor::handleToggle(GdkEventButton* const& e) {
  if (e->button == 1) {
    if (config_["dynamic-timeout"].asBool()) {
      toggleStatus(1);
    } else {
      toggleStatus();
    }

    // Make all other idle inhibitor modules update
    for (auto const& module : waybar::modules::IdleInhibitor::modules) {
      if (module != this) {
        module->update();
      }
    }
  }
  if (e->button == 3) {
    toggleStatus(0);

    // Make all other idle inhibitor modules update
    for (auto const& module : waybar::modules::IdleInhibitor::modules) {
      if (module != this) {
        module->update();
      }
    }
  }
  if (e->button == 2) {
    toggleStatus(0);
    timeout = config_["timeout"].asDouble();
  }
  ALabel::handleToggle(e);
  return true;
}

bool waybar::modules::IdleInhibitor::handleScroll(GdkEventScroll* e) {
  if (!config_["dynamic-timeout"].asBool()) {
    return true;
  }
  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }
  toggleStatus(0);
  double step = dir == SCROLL_DIR::UP ? 1 : -1;
  step *= timeout_step;
  timeout += step;
  if (timeout < 0) {
    timeout = 0;
  }
  deactivationTime = time(nullptr) + timeout * 60;

  ALabel::handleScroll(e);
  return true;
}
