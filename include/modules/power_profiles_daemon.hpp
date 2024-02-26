#pragma once

#include <fmt/format.h>

#include "ALabel.hpp"
#include "giomm/dbusproxy.h"

namespace waybar::modules {

typedef struct {
  std::string name;
  std::string driver;
} Profile;

class PowerProfilesDaemon : public ALabel {
 public:
  PowerProfilesDaemon(const std::string&, const Json::Value&);
  ~PowerProfilesDaemon();
  auto update() -> void override;
  void profileChanged_cb(const Gio::DBus::Proxy::MapChangedProperties&,
                         const std::vector<Glib::ustring>&);
  void populateInitState();
  virtual bool handleToggle(GdkEventButton* const& e);

 private:
  // Look for a profile name in the list of available profiles and
  // switch activeProfile_ to it.
  void switchToProfile_(std::string);
  // Used to toggle/display the profiles
  std::vector<Profile> availableProfiles_;
  // Points to the active profile in the profiles list
  std::vector<Profile>::iterator activeProfile_;
  // Current CSS class applied to the label
  std::string currentStyle_;
  // DBus Proxy used to track the current active profile
  Glib::RefPtr<Gio::DBus::Proxy> power_profiles_proxy_;
  sigc::connection powerProfileChangeSignal_;
};

}  // namespace waybar::modules
