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
    : ALabel(config, "power-profiles-daemon", id, "{profile}", 0, false, true) {
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
  powerProfilesProxy_ = Gio::DBus::Proxy::create_for_bus_sync(
      Gio::DBus::BusType::BUS_TYPE_SYSTEM, "net.hadess.PowerProfiles", "/net/hadess/PowerProfiles",
      "net.hadess.PowerProfiles");
  if (!powerProfilesProxy_) {
    spdlog::error("PowerProfilesDaemon: DBus error, cannot connect to net.hasdess.PowerProfile");
  } else {
    // Connect active profile callback
    powerProfileChangeSignal_ = powerProfilesProxy_->signal_properties_changed().connect(
        sigc::mem_fun(*this, &PowerProfilesDaemon::profileChangedCb));
    populateInitState();
    dp.emit();
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

PowerProfilesDaemon::~PowerProfilesDaemon() {
  if (powerProfileChangeSignal_.connected()) {
    powerProfileChangeSignal_.disconnect();
  }
  if (powerProfilesProxy_) {
    powerProfilesProxy_.reset();
  }
}

void PowerProfilesDaemon::profileChangedCb(
    const Gio::DBus::Proxy::MapChangedProperties& changedProperties,
    const std::vector<Glib::ustring>& invalidatedProperties) {
  if (auto activeProfileVariant = changedProperties.find("ActiveProfile");
      activeProfileVariant != changedProperties.end()) {
    std::string activeProfile =
        Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(activeProfileVariant->second)
            .get();
    switchToProfile(activeProfile);
    update();
  }
}

auto PowerProfilesDaemon::update() -> void {
  auto profile = (*activeProfile_);
  // Set label
  fmt::dynamic_format_arg_store<fmt::format_context> store;
  store.push_back(fmt::arg("profile", profile.name));
  label_.set_markup(fmt::vformat("⚡ {profile}", store));
  if (tooltipEnabled()) {
    label_.set_tooltip_text(fmt::format("Driver: {}", profile.driver));
  }

  // Set CSS class
  if (!currentStyle_.empty()) {
    label_.get_style_context()->remove_class(currentStyle_);
  }
  label_.get_style_context()->add_class(profile.name);
  currentStyle_ = profile.name;

  ALabel::update();
}

bool PowerProfilesDaemon::handleToggle(GdkEventButton* const& e) {
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
    powerProfilesProxy_->call_sync("org.freedesktop.DBus.Properties.Set", container);

    update();
  }
  return true;
}

}  // namespace waybar::modules
