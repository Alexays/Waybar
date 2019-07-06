#include "modules/idle_inhibitor.hpp"
#include "util/command.hpp"

namespace waybar::modules {

IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "idle_inhibitor", id, "{status}"),
      bar_(bar),
      status_("deactivated"),
      idle_inhibitor_(nullptr),
      pid_(-1) {
  args_.emplace("status", Arg{std::bind(&IdleInhibitor::getStatus, this), true});
  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &IdleInhibitor::handleToggle));
  dp.emit();
}

IdleInhibitor::~IdleInhibitor() {
  if (idle_inhibitor_ != nullptr) {
    zwp_idle_inhibitor_v1_destroy(idle_inhibitor_);
    idle_inhibitor_ = nullptr;
  }
  if (pid_ != -1) {
    kill(-pid_, 9);
    pid_ = -1;
  }
}

const std::string IdleInhibitor::getStatus() const { return status_; }

auto IdleInhibitor::update() -> void {
  label_.set_markup(
      fmt::format(format_, fmt::arg("status", status_), fmt::arg("icon", getIcon(0, status_))));
  label_.get_style_context()->add_class(status_);
  if (tooltipEnabled()) {
    label_.set_tooltip_text(status_);
  }
}

bool IdleInhibitor::handleToggle(GdkEventButton* const& e) {
  if (e->button == 1) {
    label_.get_style_context()->remove_class(status_);
    if (idle_inhibitor_ != nullptr) {
      zwp_idle_inhibitor_v1_destroy(idle_inhibitor_);
      idle_inhibitor_ = nullptr;
      status_ = "deactivated";
    } else {
      idle_inhibitor_ = zwp_idle_inhibit_manager_v1_create_inhibitor(
          Client::inst()->idle_inhibit_manager, bar_.surface);
      status_ = "activated";
    }
    click_param = status_;
  }
  ALabel::handleToggle(e);
  return true;
}

}  // namespace waybar::modules
