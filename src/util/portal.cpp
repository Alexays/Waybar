#include "util/portal.hpp"

#include <giomm/dbusproxy.h>
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
                       PORTAL_BUS_NAME, PORTAL_OBJ_PATH, PORTAL_INTERFACE),
      currentMode(Appearance::UNKNOWN) {
  refreshAppearance();
};

void waybar::Portal::refreshAppearance() {
  auto params = Glib::Variant<std::tuple<Glib::ustring, Glib::ustring>>::create(
      {PORTAL_NAMESPACE, PORTAL_KEY});
  Glib::VariantBase response;
  try {
    response = call_sync(std::string(PORTAL_INTERFACE) + ".Read", params);
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
  auto container = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(response);
  Glib::VariantBase modev;
  container.get_child(modev, 0);
  auto mode =
      Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::Variant<Glib::Variant<uint32_t>>>>(modev)
          .get()
          .get()
          .get();
  auto newMode = Appearance(mode);
  if (newMode == currentMode) {
    return;
  }
  spdlog::info("Discovered appearance '{}'", newMode);
  currentMode = newMode;
  m_signal_appearance_changed.emit(currentMode);
}

waybar::Appearance waybar::Portal::getAppearance() { return currentMode; };

void waybar::Portal::on_signal(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                               const Glib::VariantContainerBase& parameters) {
  spdlog::debug("Received signal {}", (std::string)signal_name);
  if (signal_name != "SettingChanged" || parameters.get_n_children() != 3) {
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
      Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::Variant<uint32_t>>>(valuev).get().get();
  auto newMode = Appearance(value);
  if (newMode == currentMode) {
    return;
  }
  spdlog::info("Received new appearance '{}'", newMode);
  currentMode = newMode;
  m_signal_appearance_changed.emit(currentMode);
}
