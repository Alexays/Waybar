#pragma once

#include <fmt/format.h>

#include "ALabel.hpp"
#include "giomm/dbusproxy.h"

namespace waybar::modules {

struct Profile {
  std::string name;
  std::string driver;
};

class PowerProfilesDaemon : public ALabel {
 public:
  PowerProfilesDaemon(const std::string&, const Json::Value&);
  ~PowerProfilesDaemon() override;
  auto update() -> void override;
  void profileChangedCb(const Gio::DBus::Proxy::MapChangedProperties&,
                        const std::vector<Glib::ustring>&);
  void populateInitState();
  bool handleToggle(GdkEventButton* const& e) override;

 private:
  // Look for a profile name in the list of available profiles and
  // switch activeProfile_ to it.
  void switchToProfile(std::string const&);
  // Used to toggle/display the profiles
  std::vector<Profile> availableProfiles_;
  // Points to the active profile in the profiles list
  std::vector<Profile>::iterator activeProfile_;
  // Current CSS class applied to the label
  std::string currentStyle_;
  // DBus Proxy used to track the current active profile
  Glib::RefPtr<Gio::DBus::Proxy> powerProfilesProxy_;
  sigc::connection powerProfileChangeSignal_;
};

}  // namespace waybar::modules
