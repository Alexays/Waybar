#include "util/portal.hpp"

#include <giomm/dbusproxy.h>
#include <glibmm/main.h>
#include <glibmm/variant.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>

#include "fmt/format.h"

namespace waybar {
static constexpr const char* PORTAL_BUS_NAME = "org.freedesktop.portal.Desktop";
static constexpr const char* PORTAL_OBJ_PATH = "/org/freedesktop/portal/desktop";
static constexpr const char* PORTAL_INTERFACE = "org.freedesktop.portal.Settings";
static constexpr const char* PORTAL_NAMESPACE = "org.freedesktop.appearance";
static constexpr const char* PORTAL_KEY = "color-scheme";
static constexpr unsigned APPEARANCE_HOLD_OFF_MS = 200;
}  // namespace waybar

auto fmt::formatter<waybar::Appearance>::format(waybar::Appearance c, format_context& ctx) const {
  string_view name;
  switch (c) {
    case waybar::Appearance::LIGHT:
      name = "light";
      break;
    case waybar::Appearance::DARK:
      name = "dark";
      break;
    default:
      name = "unknown";
      break;
  }
  return formatter<string_view>::format(name, ctx);
}

waybar::Portal::Portal()
    : Gio::DBus::Proxy(Gio::DBus::Connection::get_sync(Gio::DBus::BusType::BUS_TYPE_SESSION),
                       PORTAL_BUS_NAME, PORTAL_OBJ_PATH, PORTAL_INTERFACE,
                       Glib::RefPtr<Gio::DBus::InterfaceInfo>(),
                       Gio::DBus::PROXY_FLAGS_DO_NOT_AUTO_START),
      currentMode(Appearance::UNKNOWN),
      pendingMode(Appearance::UNKNOWN) {
  // Without DO_NOT_AUTO_START the proxy asks the bus to spawn the portal and
  // waits for it, so a slow portal delays the whole bar. Watch for the name
  // instead, which also covers a portal that only shows up later.
  // (A lambda, not sigc::mem_fun: Gio::DBus::Proxy is a private base here, so
  // sigc::trackable is not accessible.)
  connect_property_changed("g-name-owner", [this]() { refreshAppearance(); });
  refreshAppearance();
};

// The hold-off timer's lambda captures this
waybar::Portal::~Portal() { holdOffTimer.disconnect(); }

void waybar::Portal::refreshAppearance() {
  if (get_name_owner().empty()) {
    // Nothing is serving the portal yet; the g-name-owner handler will call us
    // back if something does.
    return;
  }
  auto params = Glib::Variant<std::tuple<Glib::ustring, Glib::ustring>>::create(
      {PORTAL_NAMESPACE, PORTAL_KEY});
  call(
      std::string(PORTAL_INTERFACE) + ".Read",
      [this](Glib::RefPtr<Gio::AsyncResult>& result) { onAppearanceReceived(result); }, params);
}

void waybar::Portal::onAppearanceReceived(Glib::RefPtr<Gio::AsyncResult>& result) {
  Glib::VariantBase response;
  try {
    response = call_finish(result);
  } catch (const Glib::Error& e) {
    spdlog::info("Unable to receive desktop appearance: {}", std::string(e.what()));
    return;
  }

  // unfortunately, the response is triple-nested, with type (v<v<uint32_t>>),
  // so we have cast thrice. This is a variation from the freedesktop standard
  // (it should only be doubly nested) but all implementations appear to do so.
  //
  // xdg-desktop-portal 1.17 will fix this issue with a new `ReadOne` method,
  // but this version is not yet released.
  // TODO(xdg-desktop-portal v1.17): switch to ReadOne
  if (!response.is_of_type(Glib::VariantType("(v)"))) {
    spdlog::error("Unexpected appearance reply type: {}", std::string(response.get_type_string()));
    return;
  }
  try {
    auto container = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(response);
    Glib::VariantBase modev;
    container.get_child(modev, 0);
    auto mode =
        Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::Variant<Glib::Variant<uint32_t>>>>(
            modev)
            .get()
            .get()
            .get();
    setAppearance(Appearance(mode));
  } catch (const std::bad_cast& e) {
    spdlog::error("Unexpected appearance variant format: {}", e.what());
    return;
  }
}

waybar::Appearance waybar::Portal::getAppearance() { return currentMode; };

void waybar::Portal::on_signal(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                               const Glib::VariantContainerBase& parameters) {
  spdlog::debug("Received signal {}", (std::string)signal_name);
  // The casts below throw on a type mismatch, which aborts the bar from a signal handler
  if (signal_name != "SettingChanged" || !parameters.is_of_type(Glib::VariantType("(ssv)"))) {
    return;
  }
  Glib::VariantBase nspcv;
  Glib::VariantBase keyv;
  Glib::VariantBase valuev;
  parameters.get_child(nspcv, 0);
  parameters.get_child(keyv, 1);
  parameters.get_child(valuev, 2);
  auto nspc = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(nspcv).get();
  auto key = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(keyv).get();
  if (nspc != PORTAL_NAMESPACE || key != PORTAL_KEY) {
    return;
  }
  auto value =
      Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::Variant<uint32_t>>>(valuev).get();
  if (!value.is_of_type(Glib::VARIANT_TYPE_UINT32)) {
    return;
  }
  setAppearance(Appearance(value.get()));
}

// Each change reloads the theme and style sheet, so the first one is announced at once and the
// ones that follow are coalesced into one per hold-off interval, for the latest value.
void waybar::Portal::setAppearance(Appearance newMode) {
  pendingMode = newMode;
  if (!holdOffTimer.connected() && announceAppearance()) {
    holdOffTimer = Glib::signal_timeout().connect([this]() { return announceAppearance(); },
                                                  APPEARANCE_HOLD_OFF_MS);
  }
}

// Returns whether there was a change, which keeps the hold-off timer running
bool waybar::Portal::announceAppearance() {
  if (pendingMode == currentMode) {
    return false;
  }
  spdlog::info("Received new appearance '{}'", pendingMode);
  currentMode = pendingMode;
  m_signal_appearance_changed.emit(currentMode);
  return true;
}
