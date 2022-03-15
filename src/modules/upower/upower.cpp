#include "modules/upower/upower.hpp"

#include <gio/gio.h>

#include <iostream>
#include <map>
#include <string>

#include "gtkmm/enums.h"
#include "gtkmm/icontheme.h"

namespace waybar::modules {
UPower::UPower(const std::string& id, const Json::Value& config)
    : AModule(config, "tray", id),
      box_(Gtk::ORIENTATION_HORIZONTAL, 0),
      icon_(),
      label_(),
      devices() {
  box_.pack_start(icon_);
  box_.pack_start(label_);
  event_box_.add(box_);

  // Icon Size
  if (config_["icon-size"].isUInt()) {
    iconSize = config_["icon-size"].asUInt();
  }
  icon_.set_pixel_size(iconSize);

  // Hide If Empty
  if (config_["hide-if-empty"].isBool()) {
    hideIfEmpty = config_["hide-if-empty"].asBool();
  }

  GError* error = NULL;
  client = up_client_new_full(NULL, &error);
  if (client == NULL) {
    throw std::runtime_error("Unable to create UPower client!");
  }

  // Connect to Login1 PrepareForSleep signal
  login1_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!login1_connection) {
    throw std::runtime_error("Unable to connect to the SYSTEM Bus!...");
  } else {
    login1_id = g_dbus_connection_signal_subscribe(login1_connection,
                                                   "org.freedesktop.login1",
                                                   "org.freedesktop.login1.Manager",
                                                   "PrepareForSleep",
                                                   "/org/freedesktop/login1",
                                                   NULL,
                                                   G_DBUS_SIGNAL_FLAGS_NONE,
                                                   prepareForSleep_cb,
                                                   this,
                                                   NULL);
  }

  g_signal_connect(client, "device-added", G_CALLBACK(deviceAdded_cb), this);
  g_signal_connect(client, "device-removed", G_CALLBACK(deviceRemoved_cb), this);
  g_signal_connect(client, "notify", G_CALLBACK(deviceNotify_cb), this);

  resetDevices();

  dp.emit();
}

UPower::~UPower() {
  if (client != NULL) g_object_unref(client);
  if (login1_id > 0) {
    g_dbus_connection_signal_unsubscribe(login1_connection, login1_id);
    login1_id = 0;
  }
}

void UPower::deviceAdded_cb(UpClient* client, UpDevice* device, gpointer data) {
  UPower* up = static_cast<UPower*>(data);
  up->addDevice(device);
  up->setDisplayDevice();
  // Update the widget
  up->dp.emit();
}
void UPower::deviceRemoved_cb(UpClient* client, const gchar* object_path, gpointer data) {
  UPower* up = static_cast<UPower*>(data);
  up->removeDevice(object_path);
  up->setDisplayDevice();
  // Update the widget
  up->dp.emit();
}
void UPower::deviceNotify_cb(gpointer data) {
  UPower* up = static_cast<UPower*>(data);
  // Update the widget
  up->dp.emit();
}
void UPower::prepareForSleep_cb(GDBusConnection* system_bus, const gchar* sender_name,
                                const gchar* object_path, const gchar* interface_name,
                                const gchar* signal_name, GVariant* parameters, gpointer data) {
  if (g_variant_is_of_type(parameters, G_VARIANT_TYPE("(b)"))) {
    gboolean sleeping;
    g_variant_get(parameters, "(b)", &sleeping);

    if (!sleeping) {
      UPower* up = static_cast<UPower*>(data);
      up->resetDevices();
    }
  }
}

void UPower::removeDevice(const std::string devicePath) { devices.erase(devicePath); }

void UPower::addDevice(UpDevice* device) {
  if (device) {
    const gchar* objectPath = up_device_get_object_path(device);
    devices[objectPath] = device;
    g_signal_connect(device, "notify", G_CALLBACK(deviceNotify_cb), this);
  }
}

void UPower::setDisplayDevice() {
  displayDevice = up_client_get_display_device(client);
  g_signal_connect(displayDevice, "notify", G_CALLBACK(deviceNotify_cb), this);
}

/** Removes all devices and adds the current devices */
void UPower::resetDevices() {
  // Removes all devices
  if (devices.size() > 0) {
    auto it = devices.cbegin();
    while (it != devices.cend()) {
      devices.erase(it++);
    }
  }

  // Adds all devices
  GPtrArray* newDevices = up_client_get_devices2(client);
  for (guint i = 0; i < newDevices->len; i++) {
    UpDevice* device = (UpDevice*)g_ptr_array_index(newDevices, i);
    if (device) addDevice(device);
  }

  setDisplayDevice();

  // Update the widget
  dp.emit();
}

auto UPower::update() -> void {
  if (devices.size() == 0 && hideIfEmpty) {
    box_.set_visible(false);
  } else {
    box_.set_visible(true);

    UpDeviceKind  kind;
    UpDeviceState state;
    double        percentage;
    gboolean      is_power_supply;
    gboolean      is_present;
    gchar*        icon_name;

    g_object_get(displayDevice,
                 "kind",
                 &kind,
                 "state",
                 &state,
                 "is-present",
                 &is_present,
                 "power-supply",
                 &is_power_supply,
                 "percentage",
                 &percentage,
                 "icon-name",
                 &icon_name,
                 NULL);

    bool displayDeviceValid =
        kind == UpDeviceKind::UP_DEVICE_KIND_BATTERY || kind == UpDeviceKind::UP_DEVICE_KIND_UPS;

    // TODO: Tooltip

    // Set percentage
    std::string percent_string =
        displayDeviceValid ? std::to_string(int(percentage) + 0.5) + "%" : "";
    label_.set_text(percent_string);

    // Set icon
    if (!Gtk::IconTheme::get_default()->has_icon(icon_name)) {
      icon_name = (char*)"battery-missing-symbolic";
    }
    icon_.set_from_icon_name(icon_name, Gtk::ICON_SIZE_INVALID);
  }

  // Call parent update
  AModule::update();
}

}  // namespace waybar::modules
