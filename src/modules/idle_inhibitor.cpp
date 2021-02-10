#include "modules/idle_inhibitor.hpp"

#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "util/command.hpp"

std::list<waybar::AModule*> waybar::modules::IdleInhibitor::modules;
bool                        waybar::modules::IdleInhibitor::status = false;

waybar::modules::IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar,
                                              const Json::Value& config)
    : ALabel(config, "idle_inhibitor", id, "{status}"),
      bar_(bar),
      idle_inhibitor_(nullptr),
      pid_(-1) {
  if (waybar::Client::inst()->idle_inhibit_manager == nullptr) {
    throw std::runtime_error("idle-inhibit not available");
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
  label_.set_markup(
      fmt::format(format_, fmt::arg("status", status_text), fmt::arg("icon", getIcon(0, status_text))));
  label_.get_style_context()->add_class(status_text);
  if (tooltipEnabled()) {
    label_.set_tooltip_text(status_text);
  }
  // Call parent update
  ALabel::update();
}

bool waybar::modules::IdleInhibitor::handleToggle(GdkEventButton* const& e) {
  if (e->button == 1) {
    status = !status;

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
