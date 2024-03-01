#include "modules/power_profiles_daemon.hpp"

// In the 80000 version of fmt library authors decided to optimize imports
// and moved declarations required for fmt::dynamic_format_arg_store in new
// header fmt/args.h
#if (FMT_VERSION >= 80000)
#include <fmt/args.h>
#else
#include <fmt/core.h>
#endif

#include <glibmm.h>
#include <glibmm/variant.h>
#include <spdlog/spdlog.h>

namespace waybar::modules {

PowerProfilesDaemon::PowerProfilesDaemon(const std::string& id, const Json::Value& config)
    : ALabel(config, "power-profiles-daemon", id, "{profile}", 0, false, true), connected_(false) {
  if (config_["format"].isString()) {
    format_ = config_["format"].asString();
  } else {
    format_ = "{icon}";
  }

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
}

PowerProfilesDaemon::~PowerProfilesDaemon() {
  if (powerProfileChangeSignal_.connected()) {
    powerProfileChangeSignal_.disconnect();
  }
  if (powerProfilesProxy_) {
    powerProfilesProxy_.reset();
  }
}

void PowerProfilesDaemon::busConnectedCb(Glib::RefPtr<Gio::AsyncResult>& r) {
  try {
    powerProfilesProxy_ = Gio::DBus::Proxy::create_for_bus_finish(r);
    using GetAllProfilesVar = Glib::Variant<std::tuple<Glib::ustring>>;
    auto callArgs = GetAllProfilesVar::create(std::make_tuple("net.hadess.PowerProfiles"));

    auto container = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(callArgs);
    powerProfilesProxy_->call("org.freedesktop.DBus.Properties.GetAll",
                              sigc::mem_fun(*this, &PowerProfilesDaemon::getAllPropsCb), container);
    // Connect active profile callback
  } catch (const std::exception& e) {
    spdlog::error("Failed to create the power profiles daemon DBus proxy");
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
    powerProfileChangeSignal_ = powerProfilesProxy_->signal_properties_changed().connect(
        sigc::mem_fun(*this, &PowerProfilesDaemon::profileChangedCb));
    populateInitState();
    dp.emit();
  } catch (const std::exception& err) {
    spdlog::error("Failed to query power-profiles-daemon via dbus: {}", err.what());
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
  Glib::ustring name;
  Glib::ustring driver;
  Profile profile;
  for (auto& variantDict : profilesVariant.get()) {
    if (auto p = variantDict.find("Profile"); p != variantDict.end()) {
      name = p->second.get();
    }
    if (auto d = variantDict.find("Driver"); d != variantDict.end()) {
      driver = d->second.get();
    }
    profile = {name, driver};
    availableProfiles_.push_back(profile);
  }

  // Find the index of the current activated mode (to toggle)
  std::string str = profileStr.get();
  switchToProfile(str);

  update();
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
      update();
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
    throw std::runtime_error("FATAL, can't find the active profile in the available profiles list");
  }
}

auto PowerProfilesDaemon::update() -> void {
  if (connected_) {
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

    ALabel::update();
  }
}

bool PowerProfilesDaemon::handleToggle(GdkEventButton* const& e) {
  if (connected_) {
    if (e->type == GdkEventType::GDK_BUTTON_PRESS && powerProfilesProxy_) {
      activeProfile_++;
      if (activeProfile_ == availableProfiles_.end()) {
        activeProfile_ = availableProfiles_.begin();
      }

      using VarStr = Glib::Variant<Glib::ustring>;
      using SetPowerProfileVar = Glib::Variant<std::tuple<Glib::ustring, Glib::ustring, VarStr>>;
      VarStr activeProfileVariant = VarStr::create(activeProfile_->name);
      auto callArgs = SetPowerProfileVar::create(
          std::make_tuple("net.hadess.PowerProfiles", "ActiveProfile", activeProfileVariant));
      auto container = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(callArgs);
      powerProfilesProxy_->call("org.freedesktop.DBus.Properties.Set",
                                sigc::mem_fun(*this, &PowerProfilesDaemon::setPropCb), container);
    }
  }
  return true;
}

void PowerProfilesDaemon::setPropCb(Glib::RefPtr<Gio::AsyncResult>& r) {
  auto _ = powerProfilesProxy_->call_finish(r);
  update();
}

}  // namespace waybar::modules
