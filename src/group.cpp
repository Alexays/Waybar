#include "group.hpp"

#include <fmt/format.h>
#include <gtkmm/enums.h>
#include <gtkmm/widget.h>

#include <util/command.hpp>

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
    : AModule(config, name, id, true, false),
      box{vertical ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL, 0},
      revealer_box{vertical ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL, 0},
      leader_revealer_box{vertical ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL, 0} {
  box.set_name(name_);
  // Clicks on a group only drive its drawer: it keeps the cursor of what is under the pointer
  if (!config_.isMember("cursor")) {
    gtk_widget_set_cursor(GTK_WIDGET(event_box_.gobj()), nullptr);
  }
  box.add_css_class("empty");
  if (!id.empty()) {
    box.add_css_class(id);
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
    drawer_left_to_right = left_to_right;
    const bool reveal_by_default =
        (drawer_config["reveal-by-default"].isBool() ? drawer_config["reveal-by-default"].asBool()
                                                     : false);

    click_to_reveal = drawer_config["click-to-reveal"].asBool();
    hide_leader_when_expanded = drawer_config["hide-leader-when-expanded"].asBool();
    always_visible_class = (drawer_config["always-visible-class"].isString()
                                ? drawer_config["always-visible-class"].asString()
                                : "");
    reveal_delay = drawer_config["reveal-delay"].asInt();

    const bool start_expanded =
        (drawer_config["start-expanded"].isBool() ? drawer_config["start-expanded"].asBool()
                                                  : false);
    empty_if_drawer_empty = (drawer_config["empty-if-drawer-empty"].isBool()
                                 ? drawer_config["empty-if-drawer-empty"].asBool()
                                 : false);

    auto transition_type = getPreferredTransitionType(vertical);

    revealer.set_transition_type(transition_type);
    revealer.set_transition_duration(transition_duration);
    leader_revealer.set_transition_type(vertical ? Gtk::RevealerTransitionType::SLIDE_DOWN
                                                 : Gtk::RevealerTransitionType::SLIDE_RIGHT);
    leader_revealer.set_transition_duration(transition_duration);
    leader_revealer.set_reveal_child(true);
    if ((click_to_reveal && reveal_by_default) || start_expanded) {
      box.set_state_flags(Gtk::StateFlags::PRELIGHT);
      revealer.set_reveal_child(true);
    } else {
      revealer.set_reveal_child(false);
    }

    revealer.add_css_class("drawer");
    leader_revealer.add_css_class("drawer-leader");

    revealer.set_child(revealer_box);
    leader_revealer.set_child(leader_revealer_box);

    if (left_to_right) {
      packEnd(revealer);
    } else {
      packStart(revealer);
    }

    revealer.property_child_revealed().signal_changed().connect([this]() {
      if (revealer.get_child_revealed()) {
        event_box_.trigger_tooltip_query();
      }
    });
  }

  event_box_.set_child(box);

  motion_ = Gtk::EventControllerMotion::create();
  motion_->signal_enter().connect([this](double, double) { handleMouseEnter(); });
  motion_->signal_leave().connect(sigc::mem_fun(*this, &Group::handleMouseLeave));
  event_box_.add_controller(motion_);
}

void Group::packStart(Gtk::Widget& widget) {
  if (box_start_.empty()) {
    box.prepend(widget);
  } else {
    box.insert_child_after(widget, *box_start_.back());
  }
  box_start_.push_back(&widget);
}

void Group::packEnd(Gtk::Widget& widget) {
  if (box_start_.empty()) {
    box.prepend(widget);
  } else {
    box.insert_child_after(widget, *box_start_.back());
  }
}

void Group::removeFromBox(Gtk::Widget& widget) {
  std::erase(box_start_, &widget);
  box.remove(widget);
}

Group::~Group() {
  // Disconnect any pending reveal timeout so it cannot fire on a destroyed
  // instance (the GLib source is not removed by sigc::connection's destructor).
  if (reveal_timeout_.connected()) {
    reveal_timeout_.disconnect();
  }
}

void Group::show_group() {
  box.set_state_flags(Gtk::StateFlags::PRELIGHT);
  revealer.set_reveal_child(true);
  if (hide_leader_when_expanded) {
    leader_revealer.set_reveal_child(false);
  }
  box.add_css_class("expanded");
}

void Group::hide_widget(Gtk::Widget& widget) {
  widget.add_css_class(add_class_to_drawer_children);
  removeFromBox(widget);
  revealer_box.append(widget);
}

void Group::show_widget(Gtk::Widget& widget) {
  widget.remove_css_class(add_class_to_drawer_children);
  revealer_box.remove(widget);
  packEnd(widget);
}

void Group::hide_current_widget_if_inactive() {
  for (auto* child = box.get_first_child(); child != nullptr;) {
    auto* next = child->get_next_sibling();
    auto* module_box = dynamic_cast<util::ModuleBox*>(child);
    if (child != &revealer && module_box != nullptr && module_box->get_child() != nullptr &&
        !module_box->get_child()->has_css_class(always_visible_class)) {
      hide_widget(*child);
    }
    child = next;
  }
}

void Group::manage_visibility(AModule* module) {
  Gtk::Widget& widget = *module;
  auto* module_box = dynamic_cast<util::ModuleBox*>(&widget);
  if (module_box == nullptr || module_box->get_child() == nullptr) {
    return;
  }
  // The box holds this widget or the revealer only
  const bool alone = box.get_first_child() == nullptr ||
                     box.get_first_child()->get_next_sibling() == nullptr ||
                     box.get_first_child()->get_next_sibling()->get_next_sibling() == nullptr;
  if (module_box->get_child()->has_css_class(always_visible_class)) {
    if (alone && widget.get_parent() != &box) {
      Group::hide_current_widget_if_inactive();
    }
    if (widget.get_parent() != &box) {
      show_widget(widget);
    }
  } else if (!alone && widget.get_parent() == &box) {
    hide_widget(widget);
  }
}

void Group::hide_group() {
  if (hide_leader_when_expanded) {
    leader_revealer.set_reveal_child(true);
  }
  box.unset_state_flags(Gtk::StateFlags::PRELIGHT);
  revealer.set_reveal_child(false);
  box.remove_css_class("expanded");
}

void Group::handleMouseEnter() {
  if (!click_to_reveal) {
    if (reveal_delay > 0) {
      if (reveal_timeout_.connected()) {
        reveal_timeout_.disconnect();
      }

      reveal_timeout_ = Glib::signal_timeout().connect(
          [this]() {
            show_group();
            return false;
          },
          reveal_delay);
    } else {
      show_group();
    }
  }
}

void Group::handleMouseLeave() {
  if (!click_to_reveal) {
    if (reveal_delay > 0 && reveal_timeout_.connected()) {
      reveal_timeout_.disconnect();
    }

    hide_group();
  }
}

bool Group::handleToggle(const ButtonEvent& e) {
  if (!click_to_reveal || e.button != 1 || e.type != ButtonEvent::Type::PRESS) {
    return false;
  }
  // Not the prelight state: GTK4 sets it on hover by itself
  if (revealer.get_reveal_child()) {
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
    for (auto* rev_child = revealer_box.get_first_child(); rev_child != nullptr;
         rev_child = rev_child->get_next_sibling()) {
      if (rev_child->get_visible()) {
        has_visible_drawer_child = true;
        break;
      }
    }
  }

  if (is_drawer && empty_if_drawer_empty) {
    has_visible_child = has_visible_drawer_child;
  } else {
    for (auto* child = box.get_first_child(); child != nullptr; child = child->get_next_sibling()) {
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

  if (has_visible_child) {
    box.remove_css_class("empty");
  } else {
    box.add_css_class("empty");
  }
}

bool Group::handleScroll(const ScrollEvent& /*e*/) {
  // no scroll.
  return true;
}

Gtk::Box& Group::getBox() { return is_drawer ? (is_first_widget ? box : revealer_box) : box; }

void Group::addWidget(AModule* module) {
  Gtk::Widget& widget = *module;

  if (is_drawer && is_first_widget && hide_leader_when_expanded) {
    leader_revealer_box.append(widget);
    if (drawer_left_to_right) {
      packStart(leader_revealer);
    } else {
      packEnd(leader_revealer);
    }
  } else if (&getBox() == &box) {
    packStart(widget);
  } else {
    getBox().append(widget);
  }

  if (is_drawer && !is_first_widget) {
    widget.add_css_class(add_class_to_drawer_children);
  }

  is_first_widget = false;

  widget.property_visible().signal_changed().connect(sigc::mem_fun(*this, &Group::update));
  update();

  if (!always_visible_class.empty()) {
    module->signal_updated.connect(sigc::mem_fun(*this, &Group::manage_visibility));
  }
}

Group::operator Gtk::Widget&() { return event_box_; }

}  // namespace waybar
