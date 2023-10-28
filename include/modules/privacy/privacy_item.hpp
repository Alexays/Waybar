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

namespace waybar::modules::privacy {

class PrivacyItem : public Gtk::Revealer {
 public:
  PrivacyItem(const Json::Value&, enum util::PipewireBackend::PrivacyNodeType privacy_type_,
              const std::string& pos);

  bool is_enabled();

  void set_in_use(bool in_use);

  void set_icon_size(uint size);

 private:
  enum util::PipewireBackend::PrivacyNodeType privacy_type;

  std::mutex mutex_;

  bool init = false;
  bool in_use = false;
  std::string lastStatus;

  // Config
  bool enabled = true;
  std::string iconName = "image-missing-symbolic";

  Gtk::Box box_;
  Gtk::Image icon_;

  void on_child_revealed_changed();
  void on_map_changed();
};

}  // namespace waybar::modules::privacy
