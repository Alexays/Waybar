#include "modules/privacy/privacy_item.hpp"

#include "glibmm/main.h"
#include "gtkmm/label.h"
#include "gtkmm/tooltip.h"

namespace waybar::modules::privacy {

PrivacyItem::PrivacyItem(const Json::Value &config_, enum PrivacyNodeType privacy_type_,
                         std::list<PrivacyNodeInfo *> *nodes_, const std::string &pos,
                         const uint icon_size, const uint transition_duration)
    : Gtk::Revealer(),
      privacy_type(privacy_type_),
      nodes(nodes_),
      signal_conn(),
      tooltip_window(Gtk::Orientation::VERTICAL, 0),
      box_(Gtk::Orientation::HORIZONTAL, 0),
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
    set_transition_type(Gtk::RevealerTransitionType::SLIDE_RIGHT);
  } else if (pos == "modules-center") {
    set_transition_type(Gtk::RevealerTransitionType::CROSSFADE);
  } else if (pos == "modules-right") {
    set_transition_type(Gtk::RevealerTransitionType::SLIDE_LEFT);
  }
  set_transition_duration(transition_duration);

  box_.set_name("privacy-item");
  box_.append(icon_);
  icon_.set_pixel_size(icon_size);
  set_child(box_);

  // Get current theme
  gtkTheme_ = Gtk::IconTheme::get_for_display(box_.get_display());

  // Icon Name
  if (config_["icon-name"].isString()) {
    iconName = config_["icon-name"].asString();
  }
  icon_.set_from_icon_name(iconName);

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
        *this), false);
  }

  // Don't show by default
  set_reveal_child(true);
  set_visible(false);
}

void PrivacyItem::update_tooltip() {
  // Removes all old nodes
  for (auto child{tooltip_window.get_last_child()}; child; child = tooltip_window.get_last_child())
    tooltip_window.remove(*child);

  for (auto *node : *nodes) {
    Gtk::Box *box = new Gtk::Box(Gtk::Orientation::HORIZONTAL, 4);

    // Set device icon
    Gtk::Image *node_icon = new Gtk::Image();
    node_icon->set_pixel_size(tooltipIconSize);
    node_icon->set_from_icon_name(node->getIconName(gtkTheme_));
    box->append(*node_icon);

    // Set model
    auto *nodeName = new Gtk::Label(node->getName());
    box->append(*nodeName);

    tooltip_window.append(*box);
  }

  tooltip_window.show();
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
