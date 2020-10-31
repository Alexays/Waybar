#include "modules/idle_inhibitor.hpp"
#include "util/command.hpp"

waybar::modules::IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar,
                                              const Json::Value& config)
    : ALabel(config, "idle_inhibitor", id, "{status}"),
      bar_(bar),
      status_("deactivated"),
      pid_(-1) {
  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &IdleInhibitor::handleToggle));
  waybar::Client::inst()->idle_inhibitor_modules.push_back(this);
  dp.emit();
}

waybar::modules::IdleInhibitor::~IdleInhibitor() {
  if (waybar::Client::inst()->idle_inhibitor != nullptr) {
    zwp_idle_inhibitor_v1_destroy(waybar::Client::inst()->idle_inhibitor);
    waybar::Client::inst()->idle_inhibitor = nullptr;
  }
  if (pid_ != -1) {
    kill(-pid_, 9);
    pid_ = -1;
  }
}

auto waybar::modules::IdleInhibitor::update() -> void {
  // Check status
  if (waybar::Client::inst()->idle_inhibitor != nullptr) {
    status_ = "activated";
  } else {
    status_ = "deactivated";
  }

  label_.set_markup(
      fmt::format(format_, fmt::arg("status", status_), fmt::arg("icon", getIcon(0, status_))));
  label_.get_style_context()->add_class(status_);
  if (tooltipEnabled()) {
    label_.set_tooltip_text(status_);
  }
  // Call parent update
  ALabel::update();
}

bool waybar::modules::IdleInhibitor::handleToggle(GdkEventButton* const& e) {
  if (e->button == 1) {
    label_.get_style_context()->remove_class(status_);
    if (waybar::Client::inst()->idle_inhibitor != nullptr) {
      zwp_idle_inhibitor_v1_destroy(waybar::Client::inst()->idle_inhibitor);
      waybar::Client::inst()->idle_inhibitor = nullptr;
      status_ = "deactivated";
    } else {
      waybar::Client::inst()->idle_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
        waybar::Client::inst()->idle_inhibit_manager, bar_.surface);
      status_ = "activated";
    }
    click_param = status_;
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
