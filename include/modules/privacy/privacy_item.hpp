#pragma once

#include <json/value.h>

#include <iostream>
#include <map>
#include <mutex>
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
              std::list<PrivacyNodeInfo *> *nodes, const std::string &pos);

  bool is_enabled();

  void set_in_use(bool in_use);

  void set_icon_size(uint size);

 private:
  enum PrivacyNodeType privacy_type;
  std::list<PrivacyNodeInfo *> *nodes;

  Gtk::Box tooltip_window;

  bool init = false;
  bool in_use = false;
  std::string lastStatus;

  // Config
  bool enabled = true;
  std::string iconName = "image-missing-symbolic";
  bool tooltip = true;
  uint tooltipIconSize = 24;

  Gtk::Box box_;
  Gtk::Image icon_;

  void on_child_revealed_changed();
  void on_map_changed();
  void update_tooltip();
};

}  // namespace waybar::modules::privacy
