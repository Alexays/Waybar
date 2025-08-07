#include "util/upower_backend.hpp"

namespace waybar::util {

UPowerBackend::UPowerBackend(std::function<void(bool)> notify_cb) {
  notify_cb_ = std::move(notify_cb);

  // Start watching DBUS
  watcherID_ =
      Gio::DBus::watch_name(Gio::DBus::BusType::BUS_TYPE_SYSTEM, "org.freedesktop.UPower",
                            sigc::mem_fun(*this, &UPowerBackend::onAppear),
                            sigc::mem_fun(*this, &UPowerBackend::onVanished),
                            Gio::DBus::BusNameWatcherFlags::BUS_NAME_WATCHER_FLAGS_AUTO_START);
  // Get DBus async connect
  Gio::DBus::Connection::get(Gio::DBus::BusType::BUS_TYPE_SYSTEM,
                             sigc::mem_fun(*this, &UPowerBackend::getConn_cb));

  // Make UPower client
  GError **gErr = NULL;
  upClient_ = up_client_new_full(NULL, gErr);
  if (upClient_ == NULL)
    spdlog::error("Upower. UPower client connection error. {}", (*gErr)->message);

  // Subscribe UPower events
  g_signal_connect(upClient_, "device-added", G_CALLBACK(deviceAdded_cb), this);
  g_signal_connect(upClient_, "device-removed", G_CALLBACK(deviceRemoved_cb), this);

  resetDevices();
}

UPowerBackend::~UPowerBackend() {
  if (upClient_ != NULL) g_object_unref(upClient_);
  if (subscrID_ > 0u) {
    conn_->signal_unsubscribe(subscrID_);
    subscrID_ = 0u;
  }
  Gio::DBus::unwatch_name(watcherID_);
  watcherID_ = 0u;
  removeDevices();
}

void UPowerBackend::removeDevices() {
  std::lock_guard<std::mutex> guard{mutex_};
  if (!devices_.empty()) {
    auto it{devices_.cbegin()};
    while (it != devices_.cend()) {
      if (G_IS_OBJECT(it->second.upDevice)) g_object_unref(it->second.upDevice);
      devices_.erase(it++);
    }
  }
}

// Removes all devices and adds the current devices
void UPowerBackend::resetDevices() {
  // Remove all devices
  removeDevices();

  // Adds all devices
  GPtrArray *newDevices = up_client_get_devices2(upClient_);
  if (newDevices != NULL)
    for (guint i{0}; i < newDevices->len; ++i) {
      UpDevice *device{(UpDevice *)g_ptr_array_index(newDevices, i)};
      if (device && G_IS_OBJECT(device)) addDevice(device);
    }
}

void UPowerBackend::addDevice(UpDevice *device) {
  std::lock_guard<std::mutex> guard{mutex_};

  if (G_IS_OBJECT(device)) {
    const gchar *objectPath{up_device_get_object_path(device)};

    // Due to the device getting cleared after this event is fired, we
    // create a new object pointing to its objectPath
    device = up_device_new();
    upDevice_output upDevice{.upDevice = device};
    gboolean ret{up_device_set_object_path_sync(device, objectPath, NULL, NULL)};
    if (!ret) {
      g_object_unref(G_OBJECT(device));
      return;
    }

    if (devices_.find(objectPath) != devices_.cend()) {
      auto upDevice{devices_[objectPath]};
      if (G_IS_OBJECT(upDevice.upDevice)) g_object_unref(upDevice.upDevice);
      devices_.erase(objectPath);
    }

    g_signal_connect(device, "notify", G_CALLBACK(deviceNotify_cb), this);
    devices_.emplace(Devices::value_type(objectPath, upDevice));
  }

  notify(true);
}

void UPowerBackend::removeDevice(const gchar *objectPath) {
  std::lock_guard<std::mutex> guard{mutex_};
  if (devices_.find(objectPath) != devices_.cend()) {
    auto upDevice{devices_[objectPath]};
    if (G_IS_OBJECT(upDevice.upDevice)) g_object_unref(upDevice.upDevice);
    devices_.erase(objectPath);
  }

  notify(true);
}

void UPowerBackend::notifyDevice(UpDevice *device) { notify(false); }

void UPowerBackend::getUpDeviceInfo(upDevice_output &upDevice_) {
  if (upDevice_.upDevice != NULL && G_IS_OBJECT(upDevice_.upDevice)) {
    g_object_get(upDevice_.upDevice, "kind", &upDevice_.kind, "state", &upDevice_.state,
                 "percentage", &upDevice_.percentage, "icon-name", &upDevice_.icon_name,
                 "time-to-empty", &upDevice_.time_empty, "time-to-full", &upDevice_.time_full,
                 "temperature", &upDevice_.temperature, "native-path", &upDevice_.nativePath,
                 "model", &upDevice_.model, NULL);
    spdlog::debug(
        "UPower. getUpDeviceInfo. kind: \"{0}\". state: \"{1}\". percentage: \"{2}\". \
icon_name: \"{3}\". time-to-empty: \"{4}\". time-to-full: \"{5}\". temperature: \"{6}\". \
native_path: \"{7}\". model: \"{8}\"",
        fmt::format_int(upDevice_.kind).str(), fmt::format_int(upDevice_.state).str(),
        upDevice_.percentage, upDevice_.icon_name, upDevice_.time_empty, upDevice_.time_full,
        upDevice_.temperature, upDevice_.nativePath, upDevice_.model);
  }
}

void UPowerBackend::onAppear(const Glib::RefPtr<Gio::DBus::Connection> &conn,
                             const Glib::ustring &name, const Glib::ustring &name_owner) {
  upRunning_ = true;
}

void UPowerBackend::onVanished(const Glib::RefPtr<Gio::DBus::Connection> &conn,
                               const Glib::ustring &name) {
  upRunning_ = false;
}

void UPowerBackend::prepareForSleep_cb(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                       const Glib::ustring &sender_name,
                                       const Glib::ustring &object_path,
                                       const Glib::ustring &interface_name,
                                       const Glib::ustring &signal_name,
                                       const Glib::VariantContainerBase &parameters) {
  if (parameters.is_of_type(Glib::VariantType("(b)"))) {
    Glib::Variant<bool> sleeping;
    parameters.get_child(sleeping, 0);
    if (!sleeping.get()) {
      resetDevices();
      sleeping_ = false;
      notify(false);
    } else
      sleeping_ = true;
  }
}

void UPowerBackend::deviceAdded_cb(UpClient *client, UpDevice *device, gpointer data) {
  UPowerBackend *up{static_cast<UPowerBackend *>(data)};
  up->addDevice(device);
}

void UPowerBackend::deviceRemoved_cb(UpClient *client, const gchar *objectPath, gpointer data) {
  UPowerBackend *up{static_cast<UPowerBackend *>(data)};
  up->removeDevice(objectPath);
}

void UPowerBackend::deviceNotify_cb(UpDevice *device, GParamSpec *pspec, gpointer data) {
  UPowerBackend *up{static_cast<UPowerBackend *>(data)};
  up->notifyDevice(device);
}

void UPowerBackend::getConn_cb(Glib::RefPtr<Gio::AsyncResult> &result) {
  try {
    conn_ = Gio::DBus::Connection::get_finish(result);
    // Subscribe DBUs events
    subscrID_ = conn_->signal_subscribe(sigc::mem_fun(*this, &UPowerBackend::prepareForSleep_cb),
                                        "org.freedesktop.login1", "org.freedesktop.login1.Manager",
                                        "PrepareForSleep", "/org/freedesktop/login1");

  } catch (const Glib::Error &e) {
    spdlog::error("Upower. DBus connection error. {}", e.what().c_str());
  }
}

}  // namespace waybar::util
