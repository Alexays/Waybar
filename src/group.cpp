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
    always_visible_class =
      (drawer_config["always-visible-class"].isString() ? drawer_config["always-visible-class"].asString()
                              : "");

    auto transition_type = getPreferredTransitionType(vertical);

    revealer.set_transition_type(transition_type);
    revealer.set_transition_duration(transition_duration);
    revealer.set_reveal_child(false);

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

void Group::hide_widget(Gtk::Widget& widget) {
  widget.get_style_context()->add_class(add_class_to_drawer_children);
  box.remove(widget);
  revealer_box.pack_start(widget, false, false);
}

void Group::show_widget(Gtk::Widget& widget) {
  widget.get_style_context()->remove_class(add_class_to_drawer_children);
  revealer_box.remove(widget);
  box.pack_end(widget, false, false);
}

void Group::hide_current_widget_if_inactive() {
  for (auto* event_box : box.get_children()) {
    if (event_box == &revealer) {
      continue;
    }
    if (auto event_box_container = dynamic_cast<Gtk::Container*>(event_box)) {
      for (auto* the_only_visible : event_box_container->get_children()) {
        if (!the_only_visible->get_style_context()->has_class(always_visible_class)) {
          hide_widget(*event_box);
        }
      }
    }
  }
}

void Group::manage_visibility(AModule* module) {
  Gtk::Widget& widget = *module;

  if (auto container = dynamic_cast<Gtk::Container*>(&widget)) {
    for (auto* base_element : container->get_children()) {
      if (base_element->get_style_context()->has_class(always_visible_class)) {
        if (box.get_children().size() == 2) {
          Group::hide_current_widget_if_inactive();
        }
        show_widget(widget);
      } else {
        // Do not hide if it's the only widget + revealer
        if (box.get_children().size() <= 2) {
          return;
        }
        hide_widget(widget);
      }
    }
  }
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
  // noop
}

Gtk::Box& Group::getBox() { return is_drawer ? (is_first_widget ? box : revealer_box) : box; }

void Group::addWidget(AModule* module) {
  Gtk::Widget& widget = *module;

  getBox().pack_start(widget, false, false);

  if (is_drawer && !is_first_widget) {
    widget.get_style_context()->add_class(add_class_to_drawer_children);
  }

  is_first_widget = false;
  
  if (!always_visible_class.empty()) {
    module->signal_updated.connect(sigc::mem_fun(*this, &Group::manage_visibility));
  }
}

Group::operator Gtk::Widget&() { return event_box_; }

}  // namespace waybar
