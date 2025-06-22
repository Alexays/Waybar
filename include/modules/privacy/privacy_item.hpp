#pragma once

#include <json/value.h>

#include <string>

#include "gtkmm/box.h"
#include "gtkmm/image.h"
#include "gtkmm/revealer.h"
#include "util/pipewire/privacy_node_info.hpp"

using waybar::util::PipewireBackend::PrivacyNodeInfo;
using waybar::util::PipewireBackend::PrivacyNodeType;

namespace waybar::modules::privacy {

class PrivacyItem : public Gtk::Revealer {
 public:
  PrivacyItem(const Json::Value &config_, enum PrivacyNodeType privacy_type_,
              std::list<PrivacyNodeInfo *> *nodes, Gtk::Orientation orientation,
              const std::string &pos, const uint icon_size, const uint transition_duration);

  enum PrivacyNodeType privacy_type;

  void set_in_use(bool in_use);

 private:
  std::list<PrivacyNodeInfo *> *nodes;

  sigc::connection signal_conn;

  Gtk::Box tooltip_window;

  bool init = false;
  bool in_use = false;

  // Config
  std::string iconName = "image-missing-symbolic";
  bool tooltip = true;
  uint tooltipIconSize = 24;

  Gtk::Box box_;
  Gtk::Image icon_;

  void update_tooltip();
};

}  // namespace waybar::modules::privacy
