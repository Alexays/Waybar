#include "modules/power_profiles_daemon.hpp"

#include <fmt/args.h>
#include <glibmm.h>
#include <glibmm/variant.h>
#include <spdlog/spdlog.h>

namespace waybar::modules {

PowerProfilesDaemon::PowerProfilesDaemon(const std::string& id, const Json::Value& config)
    : ALabel(config, "power-profiles-daemon", id, "{icon}", 0, false, true), connected_(false) {
  if (config_["tooltip-format"].isString()) {
    tooltipFormat_ = config_["tooltip-format"].asString();
  } else {
    tooltipFormat_ = "Power profile: {profile}\nDriver: {driver}";
  }
  // Fasten your seatbelt, we're up for quite a ride. The rest of the
  // init is performed asynchronously. There's 2 callbacks involved.
  // Here's the overall idea:
  // 1. Async connect to the system bus.
  // 2. In the system bus connect callback, try to call
  //    org.freedesktop.DBus.Properties.GetAll to see if
  //    power-profiles-daemon is able to respond.
  // 3. In the GetAll callback, connect the activeProfile monitoring
  //    callback, consider the init to be successful. Meaning start
  //    drawing the module.
  //
  // There's sadly no other way around that, we have to try to call a
  // method on the proxy to see whether or not something's responding
  // on the other side.

  // NOTE: the DBus adresses are under migration. They should be
  // changed to org.freedesktop.UPower.PowerProfiles at some point.
  //
  // See
  // https://gitlab.freedesktop.org/upower/power-profiles-daemon/-/releases/0.20
  //
  // The old name is still announced for now. Let's rather use the old
  // adresses for compatibility sake.
  //
  // Revisit this in 2026, systems should be updated by then.

  Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::BUS_TYPE_SYSTEM, "net.hadess.PowerProfiles",
                                   "/net/hadess/PowerProfiles", "net.hadess.PowerProfiles",
                                   sigc::mem_fun(*this, &PowerProfilesDaemon::busConnectedCb));
  // Schedule update to set the initial visibility
  dp.emit();
}

void PowerProfilesDaemon::busConnectedCb(Glib::RefPtr<Gio::AsyncResult>& r) {
  try {
    powerProfilesProxy_ = Gio::DBus::Proxy::create_for_bus_finish(r);
    using GetAllProfilesVar = Glib::Variant<std::tuple<Glib::ustring>>;
    auto callArgs = GetAllProfilesVar::create(std::make_tuple("net.hadess.PowerProfiles"));
    powerProfilesProxy_->call("org.freedesktop.DBus.Properties.GetAll",
                              sigc::mem_fun(*this, &PowerProfilesDaemon::getAllPropsCb), callArgs);
    // Connect active profile callback
  } catch (const std::exception& e) {
    spdlog::error("Failed to create the power profiles daemon DBus proxy: {}", e.what());
  } catch (const Glib::Error& e) {
    spdlog::error("Failed to create the power profiles daemon DBus proxy: {}",
                  std::string(e.what()));
  }
}

// Callback for the GetAll call.
//
// We're abusing this call to make sure power-profiles-daemon is
// available on the host. We're not really using
void PowerProfilesDaemon::getAllPropsCb(Glib::RefPtr<Gio::AsyncResult>& r) {
  try {
    auto _ = powerProfilesProxy_->call_finish(r);
    // Power-profiles-daemon responded something, we can assume it's
    // available, we can safely attach the activeProfile monitoring
    // now.
    connected_ = true;
    powerProfilesProxy_->signal_properties_changed().connect(
        sigc::mem_fun(*this, &PowerProfilesDaemon::profileChangedCb));
    populateInitState();
  } catch (const std::exception& err) {
    spdlog::error("Failed to query power-profiles-daemon via dbus: {}", err.what());
  } catch (const Glib::Error& err) {
    spdlog::error("Failed to query power-profiles-daemon via dbus: {}", std::string(err.what()));
  }
}

void PowerProfilesDaemon::populateInitState() {
  // Retrieve current active profile
  Glib::Variant<std::string> profileStr;
  powerProfilesProxy_->get_cached_property(profileStr, "ActiveProfile");

  // Retrieve profiles list, it's aa{sv}.
  using ProfilesType = std::vector<std::map<Glib::ustring, Glib::Variant<std::string>>>;
  Glib::Variant<ProfilesType> profilesVariant;
  powerProfilesProxy_->get_cached_property(profilesVariant, "Profiles");
  for (auto& variantDict : profilesVariant.get()) {
    Glib::ustring name;
    Glib::ustring driver;
    if (auto p = variantDict.find("Profile"); p != variantDict.end()) {
      name = p->second.get();
    }
    if (auto d = variantDict.find("Driver"); d != variantDict.end()) {
      driver = d->second.get();
    }
    if (!name.empty()) {
      availableProfiles_.emplace_back(std::move(name), std::move(driver));
    } else {
      spdlog::error(
          "Power profiles daemon: power-profiles-daemon sent us an empty power profile name. "
          "Something is wrong.");
    }
  }

  // Find the index of the current activated mode (to toggle)
  std::string str = profileStr.get();
  switchToProfile(str);
}

void PowerProfilesDaemon::profileChangedCb(
    const Gio::DBus::Proxy::MapChangedProperties& changedProperties,
    const std::vector<Glib::ustring>& invalidatedProperties) {
  // We're likely connected if this callback gets triggered.
  // But better be safe than sorry.
  if (connected_) {
    if (auto activeProfileVariant = changedProperties.find("ActiveProfile");
        activeProfileVariant != changedProperties.end()) {
      std::string activeProfile =
          Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(activeProfileVariant->second)
              .get();
      switchToProfile(activeProfile);
    }
  }
}

// Look for the profile str in our internal profiles list. Using a
// vector to store the profiles ain't the smartest move
// complexity-wise, but it makes toggling between the mode easy. This
// vector is 3 elements max, we'll be fine :P
void PowerProfilesDaemon::switchToProfile(std::string const& str) {
  auto pred = [str](Profile const& p) { return p.name == str; };
  this->activeProfile_ = std::find_if(availableProfiles_.begin(), availableProfiles_.end(), pred);
  if (activeProfile_ == availableProfiles_.end()) {
    spdlog::error(
        "Power profile daemon: can't find the active profile {} in the available profiles list",
        str);
  }
  dp.emit();
}

auto PowerProfilesDaemon::update() -> void {
  if (connected_ && activeProfile_ != availableProfiles_.end()) {
    auto profile = (*activeProfile_);
    // Set label
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    store.push_back(fmt::arg("profile", profile.name));
    store.push_back(fmt::arg("driver", profile.driver));
    store.push_back(fmt::arg("icon", getIcon(0, profile.name)));
    label_.set_markup(fmt::vformat(format_, store));
    if (tooltipEnabled()) {
      label_.set_tooltip_text(fmt::vformat(tooltipFormat_, store));
    }

    // Set CSS class
    if (!currentStyle_.empty()) {
      label_.get_style_context()->remove_class(currentStyle_);
    }
    label_.get_style_context()->add_class(profile.name);
    currentStyle_ = profile.name;
    event_box_.set_visible(true);
  } else {
    event_box_.set_visible(false);
  }

  ALabel::update();
}

bool PowerProfilesDaemon::handleToggle(GdkEventButton* const& e) {
  if (e->type == GdkEventType::GDK_BUTTON_PRESS && connected_) {
    if (e->button == 1) /* left click */ {
      activeProfile_++;
      if (activeProfile_ == availableProfiles_.end()) {
        activeProfile_ = availableProfiles_.begin();
      }
    } else {
      if (activeProfile_ == availableProfiles_.begin()) {
        activeProfile_ = availableProfiles_.end();
      }
      activeProfile_--;
    }

    using VarStr = Glib::Variant<Glib::ustring>;
    using SetPowerProfileVar = Glib::Variant<std::tuple<Glib::ustring, Glib::ustring, VarStr>>;
    VarStr activeProfileVariant = VarStr::create(activeProfile_->name);
    auto callArgs = SetPowerProfileVar::create(
        std::make_tuple("net.hadess.PowerProfiles", "ActiveProfile", activeProfileVariant));
    powerProfilesProxy_->call("org.freedesktop.DBus.Properties.Set",
                              sigc::mem_fun(*this, &PowerProfilesDaemon::setPropCb), callArgs);
  }
  return true;
}

void PowerProfilesDaemon::setPropCb(Glib::RefPtr<Gio::AsyncResult>& r) {
  try {
    auto _ = powerProfilesProxy_->call_finish(r);
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Failed to set the active power profile: {}", e.what());
  } catch (const Glib::Error& e) {
    spdlog::error("Failed to set the active power profile: {}", std::string(e.what()));
  }
}

}  // namespace waybar::modules
