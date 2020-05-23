#include "modules/idle_inhibitor.hpp"

#include "util/command.hpp"

namespace waybar::modules {

IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "idle_inhibitor", id, "{status}", "{status}"),
      bar_(bar),
      status_("deactivated"),
      idle_inhibitor_(nullptr),
      pid_(-1) {
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

auto IdleInhibitor::update(std::string format,
                           fmt::dynamic_format_arg_store<fmt::format_context>& args) -> void {
  // Default to status
  args.push_back(status_);
  auto statusArg = fmt::arg("status", status_);
  args.push_back(std::cref(statusArg));

  // Add status class
  label_.get_style_context()->add_class(status_);

  if (ALabel::hasFormat("icon", format)) {
    auto icon = getIcon(0, status_);
    auto iconArg = fmt::arg("icon", icon);
    args.push_back(std::cref(iconArg));
  }

  // Call parent update
  ALabel::update(format, args);
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
          waybar::Client::inst()->idle_inhibit_manager, bar_.surface);
      status_ = "activated";
    }
    click_param = status_;
  }
  ALabel::handleToggle(e);
  return true;
}

}  // namespace waybar::modules
