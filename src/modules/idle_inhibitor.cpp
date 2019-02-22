#include "modules/idle_inhibitor.hpp"


waybar::modules::IdleInhibitor::IdleInhibitor(const std::string& id, const Bar& bar, const Json::Value& config)
  : ALabel(config, "{status}"), bar_(bar), status_("deactivated"), idle_inhibitor_(nullptr)
{
  label_.set_name("idle_inhibitor");
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &IdleInhibitor::onClick));
  dp.emit();
}

waybar::modules::IdleInhibitor::~IdleInhibitor()
{
  if(idle_inhibitor_) {
    zwp_idle_inhibitor_v1_destroy(idle_inhibitor_);
    idle_inhibitor_ = nullptr;
  }
}

auto waybar::modules::IdleInhibitor::update() -> void
{
  label_.set_markup(
      fmt::format(format_, fmt::arg("status", status_),
                  fmt::arg("icon", getIcon(0, status_))));
  if(tooltipEnabled()) {
    label_.set_tooltip_text(status_);
  }
}

bool waybar::modules::IdleInhibitor::onClick(GdkEventButton* const& e)
{
  if(e->button == 1) {
    if (idle_inhibitor_) {
      zwp_idle_inhibitor_v1_destroy(idle_inhibitor_);
      idle_inhibitor_ = nullptr;
      status_ = "deactivated";
    } else {
      idle_inhibitor_ = zwp_idle_inhibit_manager_v1_create_inhibitor(
          bar_.client.idle_inhibit_manager, bar_.surface);
      status_ = "activated";
    }
  }
  dp.emit();
  return true;
}
