#include "group.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <util/command.hpp>

#include "gdkmm/device.h"
#include "gtkmm/widget.h"

namespace waybar {

Group::Group(const std::string& name, const std::string& id, const Json::Value& config,
             bool vertical)
    : AModule(config, name, id, true, true),
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

  if (!config_["drawer"].empty() && config_["drawer"].asBool()) {
    is_drawer = true;
    revealer.set_transition_type(Gtk::RevealerTransitionType::REVEALER_TRANSITION_TYPE_SLIDE_UP);
    revealer.set_transition_duration(500);
    revealer.set_reveal_child(false);

    revealer.get_style_context()->add_class("drawer");

    revealer.add(revealer_box);
    box.pack_start(revealer);

    revealer.add_events(Gdk::EventMask::ENTER_NOTIFY_MASK | Gdk::EventMask::LEAVE_NOTIFY_MASK);
    revealer.signal_enter_notify_event().connect(sigc::mem_fun(*this, &Group::hangleMouseHover));
    revealer.signal_leave_notify_event().connect(sigc::mem_fun(*this, &Group::hangleMouseHover));
  }
}

bool Group::hangleMouseHover(GdkEventCrossing* const& e) {
  spdlog::info("Mouse hover event");

  switch (e->type) {
    case GDK_ENTER_NOTIFY:
      spdlog::info("Mouse enter event");
      revealer.set_reveal_child(true);
      break;
    case GDK_LEAVE_NOTIFY:
      spdlog::info("Mouse leave event");
      revealer.set_reveal_child(false);
      break;
    default:
      spdlog::warn("Unhandled mouse hover event type: {}", (int)e->type);
      break;
  }

  return true;
}

auto Group::update() -> void {
  // noop
}

Gtk::Box& Group::getBox() { return is_drawer ? (is_first_widget ? box : revealer_box) : box; }

void Group::addWidget(Gtk::Widget& widget) {
  widget.set_has_tooltip(false);
  spdlog::info("Adding widget to group {}. Is first? {}", name_, is_first_widget);
  getBox().pack_start(widget, false, false);
  if (is_first_widget) {
    widget.add_events(Gdk::EventMask::ENTER_NOTIFY_MASK | Gdk::EventMask::LEAVE_NOTIFY_MASK);
    widget.signal_enter_notify_event().connect(sigc::mem_fun(*this, &Group::hangleMouseHover));
    widget.signal_leave_notify_event().connect(sigc::mem_fun(*this, &Group::hangleMouseHover));
  }
  is_first_widget = false;
}

Group::operator Gtk::Widget&() { return box; }

}  // namespace waybar
