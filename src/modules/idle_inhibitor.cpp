#include "modules/idle_inhibitor.hpp"
#include "util/command.hpp"

waybar::modules::IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar,
                                              const Json::Value& config)
    : ALabel(config, "idle_inhibitor", id, "{status}"),
      bar_(bar),
      pid_(-1) {
  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &IdleInhibitor::handleToggle));

  // Add this to the Client's idle_inhibitor_modules
  waybar::Client::inst()->idle_inhibitor_modules.push_back(this);

  dp.emit();
}

waybar::modules::IdleInhibitor::~IdleInhibitor() {
  if (idle_inhibitor_ != nullptr) {
    zwp_idle_inhibitor_v1_destroy(idle_inhibitor_);
    idle_inhibitor_ = nullptr;
  }

  // Remove this from the Client's idle_inhibitor_modules
  waybar::Client::inst()->idle_inhibitor_modules.remove(this);

  if (pid_ != -1) {
    kill(-pid_, 9);
    pid_ = -1;
  }
}

auto waybar::modules::IdleInhibitor::update() -> void {
  // Check status
  std::string status = waybar::Client::inst()->idle_inhibitor_status;
  if (status == "activated") {
    if (idle_inhibitor_ == nullptr) {
      idle_inhibitor_ = zwp_idle_inhibit_manager_v1_create_inhibitor(
        waybar::Client::inst()->idle_inhibit_manager, bar_.surface);
    }
  } else {
    if (idle_inhibitor_ != nullptr) {
      zwp_idle_inhibitor_v1_destroy(idle_inhibitor_);
      idle_inhibitor_ = nullptr;
    }
  }

  label_.set_markup(
      fmt::format(format_, fmt::arg("status", status), fmt::arg("icon", getIcon(0, status))));
  label_.get_style_context()->add_class(status);
  if (tooltipEnabled()) {
    label_.set_tooltip_text(status);
  }
  // Call parent update
  ALabel::update();
}

bool waybar::modules::IdleInhibitor::handleToggle(GdkEventButton* const& e) {
  if (e->button == 1) {
    std::string status = waybar::Client::inst()->idle_inhibitor_status;
    label_.get_style_context()->remove_class(status);
    if (status == "activated") {
      status = "deactivated";
    } else {
      status = "activated";
    }
    waybar::Client::inst()->idle_inhibitor_status = status;
    click_param = status;
  }

  // Make all other idle inhibitor modules update
  for (auto const& module : waybar::Client::inst()->idle_inhibitor_modules) {
    if (module != this) {
      module->update();
    }
  }

  ALabel::handleToggle(e);
  return true;
}
