#include "group.hpp"

namespace waybar {

const Gtk::RevealerTransitionType getPreferredTransitionType(bool is_vertical, bool left_to_right) {
  if (is_vertical) {
    if (left_to_right) {
      return Gtk::RevealerTransitionType::SLIDE_DOWN;
    } else {
      return Gtk::RevealerTransitionType::SLIDE_UP;
    }
  } else {
    if (left_to_right) {
      return Gtk::RevealerTransitionType::SLIDE_RIGHT;
    } else {
      return Gtk::RevealerTransitionType::SLIDE_LEFT;
    }
  }
}

Group::Group(const std::string& name, const std::string& id, const Json::Value& config,
             bool vertical)
    : AModule(config, name, id, true, true),
      box{vertical ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL, 0},
      revealer_box{vertical ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL, 0},
      controllMotion_{Gtk::EventControllerMotion::create()} {
  box.set_name(name_);
  if (!id.empty()) {
    box.get_style_context()->add_class(id);
  }

  // default orientation: orthogonal to parent
  auto orientation =
      config_["orientation"].empty() ? "orthogonal" : config_["orientation"].asString();
  if (orientation == "inherit") {
    // keep orientation passed
  } else if (orientation == "orthogonal") {
    box.set_orientation(vertical ? Gtk::Orientation::HORIZONTAL : Gtk::Orientation::VERTICAL);
  } else if (orientation == "vertical") {
    box.set_orientation(Gtk::Orientation::VERTICAL);
  } else if (orientation == "horizontal") {
    box.set_orientation(Gtk::Orientation::HORIZONTAL);
  } else {
    throw std::runtime_error("Invalid orientation value: " + orientation);
  }

  if (config_["drawer"].isObject()) {
    is_drawer = true;

    const auto& drawer_config = config_["drawer"];
    const int transition_duration =
        (drawer_config["transition-duration"].isInt() ? drawer_config["transition-duration"].asInt()
                                                      : 500);
    add_class_to_drawer_children =
        (drawer_config["children-class"].isString() ? drawer_config["children-class"].asString()
                                                    : "drawer-child");
    const bool left_to_right = (drawer_config["transition-left-to-right"].isBool()
                                    ? drawer_config["transition-left-to-right"].asBool()
                                    : true);

    auto transition_type = getPreferredTransitionType(vertical, left_to_right);

    revealer.set_transition_type(transition_type);
    revealer.set_transition_duration(transition_duration);
    revealer.set_reveal_child(false);

    revealer.get_style_context()->add_class("drawer");

    revealer.set_child(revealer_box);
    if (left_to_right)
      box.append(revealer);
    else
      box.prepend(revealer);

    addHoverHandlerTo(revealer);
  }
}

void Group::onMotionEnter(double x, double y) {
  revealer.set_reveal_child(true);
}

void Group::onMotionLeave() {
  revealer.set_reveal_child(false);
}

void Group::addHoverHandlerTo(Gtk::Widget& widget) {
  controllMotion_->set_propagation_phase(Gtk::PropagationPhase::TARGET);
  box.add_controller(controllMotion_);
  controllMotion_->signal_enter().connect(sigc::mem_fun(*this, &Group::onMotionEnter));
  controllMotion_->signal_leave().connect(sigc::mem_fun(*this, &Group::onMotionLeave));
}

auto Group::update() -> void {
  // noop
}

Gtk::Box& Group::getBox() { return is_drawer ? (is_first_widget ? box : revealer_box) : box; }

void Group::addWidget(Gtk::Widget& widget) {
  getBox().prepend(widget);

  if (is_drawer) {
    // Necessary because of GTK's hitbox detection
    addHoverHandlerTo(widget);
    if (!is_first_widget) {
      widget.get_style_context()->add_class(add_class_to_drawer_children);
    }
  }

  is_first_widget = false;
}

Group::operator Gtk::Widget&() { return box; }

}  // namespace waybar
