#include "group.hpp"

namespace waybar {

Gtk::RevealerTransitionType getPreferredTransitionType(bool is_vertical) {
  /* The transition direction of a drawer is not actually determined by the transition type,
   * but rather by the order of 'box' and 'revealer_box':
   *   'REVEALER_TRANSITION_TYPE_SLIDE_LEFT' and 'REVEALER_TRANSITION_TYPE_SLIDE_RIGHT'
   *   will result in the same thing.
   * However: we still need to differentiate between vertical and horizontal transition types.
   */

  if (is_vertical) {
    return Gtk::RevealerTransitionType::SLIDE_UP;
  }

  return Gtk::RevealerTransitionType::SLIDE_LEFT;
}

Group::Group(const std::string& name, const std::string& id, const Json::Value& config,
             bool vertical)
    : AModule(config, name, id, true, true),
      box_{vertical ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL, 0},
      revealer_box_{vertical ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL, 0} {
  box_.set_name(name_);
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }

  // default orientation: orthogonal to parent
  auto orientation =
      config_["orientation"].empty() ? "orthogonal" : config_["orientation"].asString();
  if (orientation == "inherit") {
    // keep orientation passed
  } else if (orientation == "orthogonal") {
    box_.set_orientation(vertical ? Gtk::Orientation::HORIZONTAL : Gtk::Orientation::VERTICAL);
  } else if (orientation == "vertical") {
    box_.set_orientation(Gtk::Orientation::VERTICAL);
  } else if (orientation == "horizontal") {
    box_.set_orientation(Gtk::Orientation::HORIZONTAL);
  } else {
    throw std::runtime_error("Invalid orientation value: " + orientation);
  }

  if (config_["drawer"].isObject()) {
    is_drawer_ = true;

    const auto& drawer_config = config_["drawer"];
    const int transition_duration =
        (drawer_config["transition-duration"].isInt() ? drawer_config["transition-duration"].asInt()
                                                      : 500);
    add_class_to_drawer_children_ =
        (drawer_config["children-class"].isString() ? drawer_config["children-class"].asString()
                                                    : "drawer-child");
    const bool left_to_right = (drawer_config["transition-left-to-right"].isBool()
                                    ? drawer_config["transition-left-to-right"].asBool()
                                    : true);
    click_to_reveal_ = drawer_config["click-to-reveal"].asBool();

    auto transition_type = getPreferredTransitionType(vertical);

    revealer_.set_transition_type(transition_type);
    revealer_.set_transition_duration(transition_duration);
    revealer_.set_reveal_child(false);

    revealer_.get_style_context()->add_class("drawer");

    revealer_.set_child(revealer_box_);
    if (left_to_right)
      box_.append(revealer_);
    else
      box_.prepend(revealer_);
  }

  AModule::bindEvents(box_);
}

void Group::show_group() {
  box_.set_state_flags(Gtk::StateFlags::PRELIGHT);
  revealer_.set_reveal_child(true);
}

void Group::hide_group() {
  box_.unset_state_flags(Gtk::StateFlags::PRELIGHT);
  revealer_.set_reveal_child(false);
}

void Group::handleMouseEnter(double x, double y) {
  if (!click_to_reveal_) {
    show_group();
  }
}

void Group::handleMouseLeave() {
  if (!click_to_reveal_ && AModule::controllScroll_->get_current_event()->get_crossing_detail() !=
                               Gdk::NotifyType::INFERIOR) {
    hide_group();
  }
}

void Group::handleToggle(int n_press, double dx, double dy) {
  if (click_to_reveal_ && AModule::controllClick_->get_current_button() == 1 /* left click */) {
    if (box_.get_state_flags() == Gtk::StateFlags::PRELIGHT) {
      hide_group();
    } else {
      show_group();
    }
  }
}

auto Group::update() -> void {
  // noop
}

Gtk::Box& Group::getBox() { return is_drawer_ ? (is_first_widget_ ? box_ : revealer_box_) : box_; }

void Group::addWidget(Gtk::Widget& widget) {
  getBox().prepend(widget);

  if (is_drawer_ && !is_first_widget_) {
    widget.get_style_context()->add_class(add_class_to_drawer_children_);
  }

  is_first_widget_ = false;
}

Gtk::Widget& Group::root() { return box_; }

}  // namespace waybar
