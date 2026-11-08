#include "modules/privacy/privacy_item.hpp"

#include <spdlog/spdlog.h>

#include <string>

#include "glibmm/main.h"
#include "gtkmm/label.h"
#include "gtkmm/revealer.h"
#include "gtkmm/tooltip.h"

namespace waybar::modules::privacy {

PrivacyItem::PrivacyItem(const Json::Value& config_, enum PrivacyNodeType privacy_type_,
                         Gtk::Orientation orientation, const std::string& pos, const uint icon_size,
                         const uint transition_duration)
    : Gtk::Revealer(),
      privacy_type(privacy_type_),
      tooltip_window(Gtk::Orientation::VERTICAL, 0),
      signal_conn(),
      box_(),
      icon_() {
  switch (privacy_type) {
    case util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_INPUT:
      box_.add_css_class("audio-in");
      iconName = "waybar-privacy-audio-input-symbolic";
      break;
    case util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_OUTPUT:
      box_.add_css_class("audio-out");
      iconName = "waybar-privacy-audio-output-symbolic";
      break;
    case util::PipewireBackend::PRIVACY_NODE_TYPE_VIDEO_INPUT:
      box_.add_css_class("screenshare");
      iconName = "waybar-privacy-screen-share-symbolic";
      break;
    case util::PipewireBackend::PRIVACY_NODE_TYPE_LOCATION:
      box_.add_css_class("location");
      iconName = "waybar-privacy-location-symbolic";
      break;
    case util::PipewireBackend::PRIVACY_NODE_TYPE_NONE:
      return;
  }

  // Set the reveal transition to not look weird when sliding in
  if (pos == "modules-left") {
    set_transition_type(orientation == Gtk::Orientation::HORIZONTAL
                            ? Gtk::RevealerTransitionType::SLIDE_RIGHT
                            : Gtk::RevealerTransitionType::SLIDE_DOWN);
  } else if (pos == "modules-center") {
    set_transition_type(Gtk::RevealerTransitionType::CROSSFADE);
  } else if (pos == "modules-right") {
    set_transition_type(orientation == Gtk::Orientation::HORIZONTAL
                            ? Gtk::RevealerTransitionType::SLIDE_LEFT
                            : Gtk::RevealerTransitionType::SLIDE_UP);
  }
  set_transition_duration(transition_duration);

  box_.set_name("privacy-item");

  // A center widget keeps the icon centered even if the orientation is vertical
  box_.set_center_widget(icon_);

  icon_.set_pixel_size(icon_size);
  set_child(box_);

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
  if (config_["tooltip"].isBool()) {
    tooltip = config_["tooltip"].asBool();
  }
  set_has_tooltip(tooltip);
  if (tooltip) {
    // Sets the window to use when showing the tooltip
    this->signal_query_tooltip().connect(
        sigc::track_obj(
            [this](int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip) {
              tooltip->set_custom(tooltip_window);
              return true;
            },
            *this),
        true);
  }

  // Don't show by default
  set_reveal_child(true);
  set_visible(false);
}

void PrivacyItem::update_tooltip() {
  // The nodes are managed: removing one destroys it
  while (auto* child = tooltip_window.get_first_child()) {
    tooltip_window.remove(*child);
  }

  set_tooltip();
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

void GeoCluePrivacyItem::set_tooltip() {
  auto* nodeName = Gtk::make_managed<Gtk::Label>("Location in use");
  tooltip_window.append(*nodeName);
}

void PWPrivacyItem::set_tooltip() {
  for (auto& node : *nodes) {
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);

    // Set device icon
    auto* node_icon = Gtk::make_managed<Gtk::Image>();
    node_icon->set_pixel_size(tooltipIconSize);
    node_icon->set_from_icon_name(node.getIconName());
    box->append(*node_icon);

    // Set model
    auto* nodeName = Gtk::make_managed<Gtk::Label>(node.getName());
    // The client picks the name; past 32767 px GDK cannot allocate the tooltip and crashes
    nodeName->set_max_width_chars(50);
    nodeName->set_ellipsize(Pango::EllipsizeMode::END);
    nodeName->set_single_line_mode(true);
    box->append(*nodeName);

    tooltip_window.append(*box);
  }
}

}  // namespace waybar::modules::privacy
