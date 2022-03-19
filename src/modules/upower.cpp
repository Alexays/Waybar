#include "modules/upower.hpp"

#include "gtkmm/icontheme.h"

namespace waybar::modules {
UPower::UPower(const std::string& id, const Json::Value& config)
    : AModule(config, "upower", id),
      box_(Gtk::ORIENTATION_HORIZONTAL, 0),
      icon_(),
      label_(),
      devices(),
      m_Mutex(),
      client() {
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

  resetDevices();
  setDisplayDevice();
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
void UPower::deviceRemoved_cb(UpClient* client, const gchar* objectPath, gpointer data) {
  UPower* up = static_cast<UPower*>(data);
  up->removeDevice(objectPath);
  up->setDisplayDevice();
  // Update the widget
  up->dp.emit();
}
void UPower::deviceNotify_cb(UpDevice* device, GParamSpec* pspec, gpointer data) {
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
      up->setDisplayDevice();
    }
  }
}

void UPower::removeDevice(const gchar* objectPath) {
  std::lock_guard<std::mutex> guard(m_Mutex);
  if (devices.find(objectPath) != devices.end()) {
    UpDevice* device = devices[objectPath];
    if (G_IS_OBJECT(device)) {
      g_object_unref(device);
    }
    devices.erase(objectPath);
  }
}

void UPower::addDevice(UpDevice* device, bool lockMutex) {
  if (G_IS_OBJECT(device)) {
    const gchar* objectPath = up_device_get_object_path(device);

    // Due to the device getting cleared after this event is fired, we
    // create a new object pointing to its objectPath
    gboolean ret;
    device = up_device_new();
    ret = up_device_set_object_path_sync(device, objectPath, NULL, NULL);
    if (!ret) {
      g_object_unref(G_OBJECT(device));
      return;
    }

    if (lockMutex) std::lock_guard<std::mutex> guard(m_Mutex);

    if (devices.find(objectPath) != devices.end()) {
      UpDevice* device = devices[objectPath];
      if (G_IS_OBJECT(device)) {
        g_object_unref(device);
      }
      devices.erase(objectPath);
    }

    g_signal_connect(device, "notify", G_CALLBACK(deviceNotify_cb), this);
    devices.emplace(Devices::value_type(objectPath, device));
  }
}

void UPower::setDisplayDevice() {
  std::lock_guard<std::mutex> guard(m_Mutex);
  displayDevice = up_client_get_display_device(client);
  g_signal_connect(displayDevice, "notify", G_CALLBACK(deviceNotify_cb), this);
}

/** Removes all devices and adds the current devices */
void UPower::resetDevices() {
  std::lock_guard<std::mutex> guard(m_Mutex);
  // Removes all devices
  if (!devices.empty()) {
    auto it = devices.cbegin();
    while (it != devices.cend()) {
      if (G_IS_OBJECT(it->second)) {
        g_object_unref(it->second);
      }
      devices.erase(it++);
    }
  }

  // Adds all devices
  GPtrArray* newDevices = up_client_get_devices2(client);
  for (guint i = 0; i < newDevices->len; i++) {
    UpDevice* device = (UpDevice*)g_ptr_array_index(newDevices, i);
    if (device) addDevice(device, false);
  }

  // Update the widget
  dp.emit();
}

auto UPower::update() -> void {
  std::lock_guard<std::mutex> guard(m_Mutex);
  if (devices.size() == 0 && hideIfEmpty) {
    event_box_.set_visible(false);
  } else {
    event_box_.set_visible(true);

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
        displayDeviceValid ? std::to_string(int(percentage + 0.5)) + "%" : "";
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
