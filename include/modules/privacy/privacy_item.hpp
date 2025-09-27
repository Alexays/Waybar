#pragma once

#include <json/value.h>

#include <string>

#include "gtkmm/box.h"
#include "gtkmm/image.h"
#include "gtkmm/revealer.h"
#include "modules/privacy/privacy.hpp"
#include "util/pipewire/privacy_node_info.hpp"

using waybar::util::PipewireBackend::PrivacyNodeType;

namespace waybar::modules::privacy {

class PrivacyItem : public Gtk::Revealer {
 protected:
  PrivacyItem(const Json::Value& config_, enum PrivacyNodeType privacy_type_,
              Gtk::Orientation orientation, const std::string& pos, const uint icon_size,
              const uint transition_duration);

 public:
  virtual void set_tooltip() = 0;

  enum PrivacyNodeType privacy_type;

  void set_in_use(bool in_use);

  uint tooltipIconSize = 24;
  Gtk::Box tooltip_window;

 private:
  sigc::connection signal_conn;

  bool init = false;
  bool in_use = false;

  // Config
  std::string iconName = "image-missing-symbolic";
  bool tooltip = true;

  Gtk::Box box_;
  Gtk::Image icon_;

  void update_tooltip();
};

class GeoCluePrivacyItem : public PrivacyItem {
 public:
  GeoCluePrivacyItem(const Json::Value& config_, Gtk::Orientation orientation,
                     const std::string& pos, const uint icon_size, const uint transition_duration)
      : PrivacyItem(config_, util::PipewireBackend::PRIVACY_NODE_TYPE_LOCATION, orientation, pos,
                    icon_size, transition_duration) {}

  void set_tooltip() override;
};

class PWPrivacyItem : public PrivacyItem {
 public:
  PWPrivacyItem(const Json::Value& config_, enum PrivacyNodeType privacy_type_,
                std::list<PWPrivacyNodeInfo*>* nodes_, Gtk::Orientation orientation,
                const std::string& pos, const uint icon_size, const uint transition_duration)
      : PrivacyItem(config_, privacy_type_, orientation, pos, icon_size, transition_duration),
        nodes(nodes_) {}

  void set_tooltip() override;

 private:
  std::list<PWPrivacyNodeInfo*>* nodes;
};

}  // namespace waybar::modules::privacy
