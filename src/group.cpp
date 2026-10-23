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
    return Gtk::RevealerTransitionType::SLIDE_UP;
  }

  return Gtk::RevealerTransitionType::SLIDE_LEFT;
}

Group::Group(const std::string& name, const std::string& id, const Json::Value& config,
             bool vertical)
    : AModule(config, name, id, true, false),
      m_box_{vertical ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL, 0},
      m_revealer_box_{vertical ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL, 0} {
  w_ = &m_box_;
  m_box_.set_name(name_);
  m_box_.get_style_context()->add_class("empty");
  if (!id.empty()) {
    m_box_.get_style_context()->add_class(id);
  }

  // default orientation: orthogonal to parent
  auto orientation =
      config_["orientation"].empty() ? "orthogonal" : config_["orientation"].asString();
  if (orientation == "inherit") {
    // keep orientation passed
  } else if (orientation == "orthogonal") {
    m_box_.set_orientation(vertical ? Gtk::Orientation::HORIZONTAL : Gtk::Orientation::VERTICAL);
  } else if (orientation == "vertical") {
    m_box_.set_orientation(Gtk::Orientation::VERTICAL);
  } else if (orientation == "horizontal") {
    m_box_.set_orientation(Gtk::Orientation::HORIZONTAL);
  } else {
    throw std::runtime_error("Invalid orientation value: " + orientation);
  }

  if (config_["drawer"].isObject()) {
    m_is_drawer_ = true;

    const auto& drawer_config = config_["drawer"];
    const int transition_duration =
        (drawer_config["transition-duration"].isInt() ? drawer_config["transition-duration"].asInt()
                                                      : 500);
    m_add_class_to_drawer_children_ =
        (drawer_config["children-class"].isString() ? drawer_config["children-class"].asString()
                                                    : "drawer-child");
    const bool left_to_right = (drawer_config["transition-left-to-right"].isBool()
                                    ? drawer_config["transition-left-to-right"].asBool()
                                    : true);
    const bool reveal_by_default =
        (drawer_config["reveal-by-default"].isBool() ? drawer_config["reveal-by-default"].asBool()
                                                     : false);

    m_click_to_reveal_ = drawer_config["click-to-reveal"].asBool();
    m_always_visible_class_ = (drawer_config["always-visible-class"].isString()
                                   ? drawer_config["always-visible-class"].asString()
                                   : "");
    m_reveal_delay_ = drawer_config["reveal-delay"].asInt();

    const bool start_expanded =
        (drawer_config["start-expanded"].isBool() ? drawer_config["start-expanded"].asBool()
                                                  : false);
    m_empty_if_drawer_empty_ = (drawer_config["empty-if-drawer-empty"].isBool()
                                    ? drawer_config["empty-if-drawer-empty"].asBool()
                                    : false);

    auto transition_type = getPreferredTransitionType(vertical);

    m_revealer_.set_transition_type(transition_type);
    m_revealer_.set_transition_duration(transition_duration);
    if ((m_click_to_reveal_ && reveal_by_default) || start_expanded) {
      m_box_.set_state_flags(Gtk::StateFlags::PRELIGHT);
      m_revealer_.set_reveal_child(true);
    } else {
      m_revealer_.set_reveal_child(false);
    }

    m_revealer_.get_style_context()->add_class("drawer");
    m_revealer_.set_child(m_revealer_box_);

    if (left_to_right) {
      m_box_.append(m_revealer_);
    } else {
      m_box_.prepend(m_revealer_);
    }
  }

  bindEvents(m_box_);
}

Group::~Group() {
  // Disconnect any pending reveal timeout so it cannot fire on a destroyed
  // instance (the GLib source is not removed by sigc::connection's destructor).
  if (m_reveal_timeout_.connected()) {
    m_reveal_timeout_.disconnect();
  }
}

void Group::show_group() {
  m_box_.set_state_flags(Gtk::StateFlags::PRELIGHT);
  m_revealer_.set_reveal_child(true);
  m_box_.get_style_context()->add_class("expanded");
}

void Group::hide_widget(Gtk::Widget& widget) {
  widget.get_style_context()->add_class(m_add_class_to_drawer_children_);
  m_box_.remove(widget);
  m_revealer_box_.append(widget);
}

void Group::show_widget(Gtk::Widget& widget) {
  widget.get_style_context()->remove_class(m_add_class_to_drawer_children_);
  m_revealer_box_.remove(widget);
  m_box_.append(widget);
}

void Group::hide_current_widget_if_inactive() {
  for (auto* widget : m_box_.get_children()) {
    if (widget == &m_revealer_) {
      continue;
    }
    for (auto* the_only_visible : widget->get_children()) {
      if (!the_only_visible->get_style_context()->has_class(m_always_visible_class_)) {
        hide_widget(*widget);
      }
    }
  }
}

void Group::manage_visibility(AModule* module) {
  Gtk::Widget& widget = *module;

  for (auto* base_element : widget.get_children()) {
    if (base_element->get_style_context()->has_class(m_always_visible_class_)) {
      if (m_box_.get_children().size() == 2) {
        Group::hide_current_widget_if_inactive();
      }
      show_widget(widget);
    } else {
      // Do not hide if it's the only widget + revealer
      if (m_box_.get_children().size() <= 2) {
        return;
      }
      hide_widget(widget);
    }
  }
}

void Group::hide_group() {
  m_box_.unset_state_flags(Gtk::StateFlags::PRELIGHT);
  m_revealer_.set_reveal_child(false);
  m_box_.get_style_context()->remove_class("expanded");
}

void Group::handleMouseEnter(double x, double y) {
  if (!m_click_to_reveal_) {
    if (m_reveal_delay_ > 0) {
      if (m_reveal_timeout_.connected()) {
        m_reveal_timeout_.disconnect();
      }

      m_reveal_timeout_ = Glib::signal_timeout().connect(
          [this]() {
            show_group();
            return false;
          },
          m_reveal_delay_);
    } else {
      show_group();
    }
  }
  return;
}

void Group::handleMouseLeave() {
  if (!m_click_to_reveal_ &&
      controller_motion_->get_current_event()->get_crossing_detail() != Gdk::NotifyType::INFERIOR) {
    if (m_reveal_delay_ > 0 && m_reveal_timeout_.connected()) {
      m_reveal_timeout_.disconnect();
    }

    hide_group();
  }
  return;
}

void Group::handlePress(int n_press, double x, double y) {
  if (!m_click_to_reveal_ || gesture_click_->get_current_event()->get_button() != 1) {
    return;
  }
  if ((m_box_.get_state_flags() & Gtk::StateFlags::PRELIGHT) != Gtk::StateFlags{}) {
    hide_group();
  } else {
    show_group();
  }
  return;
}

auto Group::doUpdate() -> void {
  bool has_visible_child{false};
  bool has_visible_drawer_child{false};

  if (m_is_drawer_) {
    for (auto* rev_child : m_revealer_box_.get_children()) {
      if (rev_child->get_visible()) {
        has_visible_drawer_child = true;
        break;
      }
    }
  }

  if (m_is_drawer_ && m_empty_if_drawer_empty_) {
    has_visible_child = has_visible_drawer_child;
  } else {
    for (auto* child : m_box_.get_children()) {
      if (child == &m_revealer_) {
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

  auto style = m_box_.get_style_context();
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

bool Group::handleScroll(double dx, double dy) {
  // no scroll.
  return true;
}

Gtk::Box& Group::getBox() {
  return m_is_drawer_ ? (m_is_first_widget_ ? m_box_ : m_revealer_box_) : m_box_;
}

void Group::addWidget(AModule* module) {
  Gtk::Widget& widget = *module;

  getBox().append(widget);

  if (m_is_drawer_ && !m_is_first_widget_) {
    widget.get_style_context()->add_class(m_add_class_to_drawer_children_);
  }

  m_is_first_widget_ = false;

  widget.property_visible().signal_changed().connect(sigc::mem_fun(*this, &Group::doUpdate));
  doUpdate();

  if (!m_always_visible_class_.empty()) {
    module->signal_updated().connect(sigc::mem_fun(*this, &Group::manage_visibility));
  }
}

}  // namespace waybar
