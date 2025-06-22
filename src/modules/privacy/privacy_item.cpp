#include "modules/privacy/privacy_item.hpp"

#include <spdlog/spdlog.h>

#include <string>

#include "glibmm/main.h"
#include "gtkmm/label.h"
#include "gtkmm/revealer.h"
#include "gtkmm/tooltip.h"
#include "util/pipewire/privacy_node_info.hpp"

namespace waybar::modules::privacy {

PrivacyItem::PrivacyItem(const Json::Value &config_, enum PrivacyNodeType privacy_type_,
                         std::list<PrivacyNodeInfo *> *nodes_, Gtk::Orientation orientation,
                         const std::string &pos, const uint icon_size,
                         const uint transition_duration)
    : Gtk::Revealer(),
      privacy_type(privacy_type_),
      nodes(nodes_),
      signal_conn(),
      tooltip_window(Gtk::ORIENTATION_VERTICAL, 0),
      box_(Gtk::ORIENTATION_HORIZONTAL, 0),
      icon_() {
  switch (privacy_type) {
    case util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_INPUT:
      box_.get_style_context()->add_class("audio-in");
      iconName = "waybar-privacy-audio-input-symbolic";
      break;
    case util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_OUTPUT:
      box_.get_style_context()->add_class("audio-out");
      iconName = "waybar-privacy-audio-output-symbolic";
      break;
    case util::PipewireBackend::PRIVACY_NODE_TYPE_VIDEO_INPUT:
      box_.get_style_context()->add_class("screenshare");
      iconName = "waybar-privacy-screen-share-symbolic";
      break;
    default:
    case util::PipewireBackend::PRIVACY_NODE_TYPE_NONE:
      return;
  }

  // Set the reveal transition to not look weird when sliding in
  if (pos == "modules-left") {
    set_transition_type(orientation == Gtk::ORIENTATION_HORIZONTAL
                            ? Gtk::REVEALER_TRANSITION_TYPE_SLIDE_RIGHT
                            : Gtk::REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  } else if (pos == "modules-center") {
    set_transition_type(Gtk::REVEALER_TRANSITION_TYPE_CROSSFADE);
  } else if (pos == "modules-right") {
    set_transition_type(orientation == Gtk::ORIENTATION_HORIZONTAL
                            ? Gtk::REVEALER_TRANSITION_TYPE_SLIDE_LEFT
                            : Gtk::REVEALER_TRANSITION_TYPE_SLIDE_UP);
  }
  set_transition_duration(transition_duration);

  box_.set_name("privacy-item");

  // We use `set_center_widget` instead of `add` to make sure the icon is
  // centered even if the orientation is vertical
  box_.set_center_widget(icon_);

  icon_.set_pixel_size(icon_size);
  add(box_);

  // Icon Name
  if (config_["icon-name"].isString()) {
    iconName = config_["icon-name"].asString();
  }
  icon_.set_from_icon_name(iconName, Gtk::ICON_SIZE_INVALID);

  // Tooltip Icon Size
  if (config_["tooltip-icon-size"].isUInt()) {
    tooltipIconSize = config_["tooltip-icon-size"].asUInt();
  }
  // Tooltip
  if (config_["tooltip"].isString()) {
    tooltip = config_["tooltip"].asBool();
  }
  set_has_tooltip(tooltip);
  if (tooltip) {
    // Sets the window to use when showing the tooltip
    update_tooltip();
    this->signal_query_tooltip().connect(sigc::track_obj(
        [this](int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip> &tooltip) {
          tooltip->set_custom(tooltip_window);
          return true;
        },
        *this));
  }

  // Don't show by default
  set_reveal_child(true);
  set_visible(false);
}

void PrivacyItem::update_tooltip() {
  // Removes all old nodes
  for (auto *child : tooltip_window.get_children()) {
    tooltip_window.remove(*child);
    // despite the remove, still needs a delete to prevent memory leak. Speculating that this might
    // work differently in GTK4.
    delete child;
  }
  for (auto *node : *nodes) {
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL, 4);

    // Set device icon
    auto *node_icon = Gtk::make_managed<Gtk::Image>();
    node_icon->set_pixel_size(tooltipIconSize);
    node_icon->set_from_icon_name(node->getIconName(), Gtk::ICON_SIZE_INVALID);
    box->add(*node_icon);

    // Set model
    auto *nodeName = Gtk::make_managed<Gtk::Label>(node->getName());
    box->add(*nodeName);

    tooltip_window.add(*box);
  }

  tooltip_window.show_all();
}

void PrivacyItem::set_in_use(bool in_use) {
  if (in_use) {
    update_tooltip();
  }

  if (this->in_use == in_use && init) return;

  if (init) {
    // Disconnect any previous connection so that it doesn't get activated in
    // the future, hiding the module when it should be visible
    signal_conn.disconnect();

    this->in_use = in_use;
    guint duration = 0;
    if (this->in_use) {
      set_visible(true);
    } else {
      set_reveal_child(false);
      duration = get_transition_duration();
    }

    signal_conn = Glib::signal_timeout().connect(sigc::track_obj(
                                                     [this] {
                                                       if (this->in_use) {
                                                         set_reveal_child(true);
                                                       } else {
                                                         set_visible(false);
                                                       }
                                                       return false;
                                                     },
                                                     *this),
                                                 duration);
  } else {
    set_visible(false);
    set_reveal_child(false);
  }
  this->init = true;
}

}  // namespace waybar::modules::privacy
