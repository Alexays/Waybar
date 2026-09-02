#include "util/geoclue_backend.hpp"

#include <giomm/dbuswatchname.h>

#include <string>

namespace waybar::util::GeoClueBackend {

GeoClueBackend::GeoClueBackend(PrivateConstructorTag tag)
    : watcherID_(0), signal_conn(), connected(false), location_in_use(false) {
  // Start watching DBUS
  watcherID_ =
      Gio::DBus::watch_name(Gio::DBus::BusType::BUS_TYPE_SYSTEM, "org.freedesktop.GeoClue2",
                            sigc::mem_fun(*this, &GeoClueBackend::onAppear),
                            sigc::mem_fun(*this, &GeoClueBackend::onVanished),
                            Gio::DBus::BusNameWatcherFlags::BUS_NAME_WATCHER_FLAGS_AUTO_START);
}

GeoClueBackend::~GeoClueBackend() {
  if (watcherID_ > 0) {
    Gio::DBus::unwatch_name(watcherID_);
    watcherID_ = 0;
  }
  signal_conn.disconnect();
}

std::shared_ptr<GeoClueBackend> GeoClueBackend::getInstance() {
  PrivateConstructorTag tag;
  return std::make_shared<GeoClueBackend>(tag);
}

void GeoClueBackend::propertyChanged(
    const Gio::DBus::Proxy::MapChangedProperties& changedProperties,
    const std::vector<Glib::ustring>& invalidatedProperties) {
  if (!connected) {
    return;
  }

  if (auto in_use_variant = changedProperties.find("InUse");
      in_use_variant != changedProperties.end()) {
    bool in_use =
        Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(in_use_variant->second).get();
    location_in_use.store(in_use);
    in_use_changed_signal_event.emit();
  }
}

void GeoClueBackend::onAppear(const Glib::RefPtr<Gio::DBus::Connection>& conn,
                              const Glib::ustring& name, const Glib::ustring& name_owner) {
  location_in_use.store(false);
  signal_conn.disconnect();
  connected = true;

  proxy = Gio::DBus::Proxy::create_sync(conn, "org.freedesktop.GeoClue2",
                                        "/org/freedesktop/GeoClue2/Manager",
                                        "org.freedesktop.GeoClue2.Manager");

  // Subscribe DBus events
  signal_conn = proxy->signal_properties_changed().connect(
      sigc::mem_fun(*this, &GeoClueBackend::propertyChanged));

  // Get the initial value
  auto callArgs = Glib::Variant<std::tuple<Glib::ustring, Glib::ustring>>::create(
      std::make_tuple("org.freedesktop.GeoClue2.Manager", "InUse"));
  Glib::VariantContainerBase base =
      proxy->call_sync("org.freedesktop.DBus.Properties.Get", callArgs);
  if (base.is_of_type(Glib::VariantType("(v)"))) {
    Glib::Variant<std::tuple<bool>> variant_value;
    base.get_child(variant_value, 0);
    auto [value] = variant_value.get();
    if (location_in_use != value) {
      location_in_use.store(value);
    }
  }

  in_use_changed_signal_event.emit();
}

void GeoClueBackend::onVanished(const Glib::RefPtr<Gio::DBus::Connection>& conn,
                                const Glib::ustring& name) {
  signal_conn.disconnect();
  connected = false;

  location_in_use.store(false);
  in_use_changed_signal_event.emit();
}

}  // namespace waybar::util::GeoClueBackend
