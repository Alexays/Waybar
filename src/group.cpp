#include "group.hpp"

#include <fmt/format.h>

#include <util/command.hpp>

#include "gtkmm/enums.h"
#include "gtkmm/widget.h"

namespace waybar {

Gtk::RevealerTransitionType getPreferredTransitionType(bool is_vertical) {
  /* The transition direction of a drawer is not actually determined by the transition type,
   * but rather by the order of 'box' and 'revealer_box':
   *   'REVEALER_TRANSITION_TYPE_SLIDE_LEFT' and 'REVEALER_TRANSITION_TYPE_SLIDE_RIGHT'
   *   will result in the same thing.
   * However: we still need to differentiate between vertical and horizontal transition types.
   */

  if (is_vertical) {
    return Gtk::RevealerTransitionType::REVEALER_TRANSITION_TYPE_SLIDE_UP;
  }

  return Gtk::RevealerTransitionType::REVEALER_TRANSITION_TYPE_SLIDE_LEFT;
}

Group::Group(const std::string& name, const std::string& id, const Json::Value& config,
             bool vertical)
    : AModule(config, name, id, true, false),
      box{vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0},
      revealer_box{vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0} {
  box.set_name(name_);
  box.get_style_context()->add_class("empty");
  if (!id.empty()) {
    box.get_style_context()->add_class(id);
  }

  // default orientation: orthogonal to parent
  auto orientation =
      config_["orientation"].empty() ? "orthogonal" : config_["orientation"].asString();
  if (orientation == "inherit") {
    // keep orientation passed
  } else if (orientation == "orthogonal") {
    box.set_orientation(vertical ? Gtk::ORIENTATION_HORIZONTAL : Gtk::ORIENTATION_VERTICAL);
  } else if (orientation == "vertical") {
    box.set_orientation(Gtk::ORIENTATION_VERTICAL);
  } else if (orientation == "horizontal") {
    box.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
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
    click_to_reveal = drawer_config["click-to-reveal"].asBool();

    const bool start_expanded =
        (drawer_config["start-expanded"].isBool() ? drawer_config["start-expanded"].asBool()
                                                  : false);
    empty_if_drawer_empty = (drawer_config["empty-if-drawer-empty"].isBool()
                                 ? drawer_config["empty-if-drawer-empty"].asBool()
                                 : false);

    auto transition_type = getPreferredTransitionType(vertical);

    revealer.set_transition_type(transition_type);
    revealer.set_transition_duration(transition_duration);
    revealer.set_reveal_child(start_expanded);

    if (start_expanded) {
      box.set_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
    }

    revealer.get_style_context()->add_class("drawer");

    revealer.add(revealer_box);

    if (left_to_right) {
      box.pack_end(revealer);
    } else {
      box.pack_start(revealer);
    }
  }

  event_box_.add(box);
}

void Group::show_group() {
  box.set_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
  revealer.set_reveal_child(true);
}

void Group::hide_group() {
  box.unset_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
  revealer.set_reveal_child(false);
}

bool Group::handleMouseEnter(GdkEventCrossing* const& e) {
  if (!click_to_reveal) {
    show_group();
  }
  return false;
}

bool Group::handleMouseLeave(GdkEventCrossing* const& e) {
  if (!click_to_reveal && e->detail != GDK_NOTIFY_INFERIOR) {
    hide_group();
  }
  return false;
}

bool Group::handleToggle(GdkEventButton* const& e) {
  if (!click_to_reveal || e->button != 1) {
    return false;
  }
  if ((box.get_state_flags() & Gtk::StateFlags::STATE_FLAG_PRELIGHT) != 0U) {
    hide_group();
  } else {
    show_group();
  }
  return true;
}

auto Group::update() -> void {
  bool has_visible_child = false;
  bool has_visible_drawer_child = false;

  if (is_drawer) {
    for (auto* rev_child : revealer_box.get_children()) {
      if (rev_child->get_visible()) {
        has_visible_drawer_child = true;
        break;
      }
    }
  }

  if (is_drawer && empty_if_drawer_empty) {
    has_visible_child = has_visible_drawer_child;
  } else {
    for (auto* child : box.get_children()) {
      if (child == &revealer) {
        if (has_visible_drawer_child) {
          has_visible_child = true;
          break;
        }
      } else if (child->get_visible()) {
        has_visible_child = true;
        break;
      }
    }
  }

  auto style = box.get_style_context();
  if (has_visible_child) {
    if (style->has_class("empty")) {
      style->remove_class("empty");
    }
  } else {
    if (!style->has_class("empty")) {
      style->add_class("empty");
    }
  }
}

bool Group::handleScroll(GdkEventScroll* e) {
  // no scroll.
  return true;
}

Gtk::Box& Group::getBox() { return is_drawer ? (is_first_widget ? box : revealer_box) : box; }

void Group::addWidget(Gtk::Widget& widget) {
  getBox().pack_start(widget, false, false);

  if (is_drawer && !is_first_widget) {
    widget.get_style_context()->add_class(add_class_to_drawer_children);
  }

  is_first_widget = false;
  widget.property_visible().signal_changed().connect(sigc::mem_fun(*this, &Group::update));
  update();
}

Group::operator Gtk::Widget&() { return event_box_; }

}  // namespace waybar
