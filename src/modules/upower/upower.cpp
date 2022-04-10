#include "modules/upower/upower.hpp"

#include <fmt/core.h>

#include <cstring>
#include <string>

#include "gtkmm/icontheme.h"
#include "gtkmm/label.h"
#include "gtkmm/tooltip.h"
#include "modules/upower/upower_tooltip.hpp"

namespace waybar::modules::upower {
UPower::UPower(const std::string& id, const Json::Value& config)
    : AModule(config, "upower", id),
      box_(Gtk::ORIENTATION_HORIZONTAL, 0),
      icon_(),
      label_(),
      devices(),
      m_Mutex(),
      client(),
      showAltText(false) {
  box_.pack_start(icon_);
  box_.pack_start(label_);
  box_.set_name(name_);
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

  // Format
  if (config_["format"].isString()) {
    format = config_["format"].asString();
  }

  // Format Alt
  if (config_["format-alt"].isString()) {
    format_alt = config_["format-alt"].asString();
  }

  // Tooltip Spacing
  if (config_["tooltip-spacing"].isUInt()) {
    tooltip_spacing = config_["tooltip-spacing"].asUInt();
  }

  // Tooltip Padding
  if (config_["tooltip-padding"].isUInt()) {
    tooltip_padding = config_["tooltip-padding"].asUInt();
  }

  // Tooltip
  if (config_["tooltip"].isBool()) {
    tooltip_enabled = config_["tooltip"].asBool();
  }
  box_.set_has_tooltip(tooltip_enabled);
  if (tooltip_enabled) {
    // Sets the window to use when showing the tooltip
    upower_tooltip = new UPowerTooltip(iconSize, tooltip_spacing, tooltip_padding);
    box_.set_tooltip_window(*upower_tooltip);
    box_.signal_query_tooltip().connect(sigc::mem_fun(*this, &UPower::show_tooltip_callback));
  }

  upowerWatcher_id = g_bus_watch_name(G_BUS_TYPE_SYSTEM, "org.freedesktop.UPower",
                                      G_BUS_NAME_WATCHER_FLAGS_AUTO_START, upowerAppear,
                                      upowerDisappear, this, NULL);

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
    login1_id = g_dbus_connection_signal_subscribe(
        login1_connection, "org.freedesktop.login1", "org.freedesktop.login1.Manager",
        "PrepareForSleep", "/org/freedesktop/login1", NULL, G_DBUS_SIGNAL_FLAGS_NONE,
        prepareForSleep_cb, this, NULL);
  }

  event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &UPower::handleToggle));

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
  g_bus_unwatch_name(upowerWatcher_id);
  removeDevices();
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
void UPower::upowerAppear(GDBusConnection* conn, const gchar* name, const gchar* name_owner,
                          gpointer data) {
  UPower* up = static_cast<UPower*>(data);
  up->upowerRunning = true;
  up->event_box_.set_visible(true);
}
void UPower::upowerDisappear(GDBusConnection* conn, const gchar* name, gpointer data) {
  UPower* up = static_cast<UPower*>(data);
  up->upowerRunning = false;
  up->event_box_.set_visible(false);
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

void UPower::addDevice(UpDevice* device) {
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

    std::lock_guard<std::mutex> guard(m_Mutex);

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

void UPower::removeDevices() {
  std::lock_guard<std::mutex> guard(m_Mutex);
  if (!devices.empty()) {
    auto it = devices.cbegin();
    while (it != devices.cend()) {
      if (G_IS_OBJECT(it->second)) {
        g_object_unref(it->second);
      }
      devices.erase(it++);
    }
  }
}

/** Removes all devices and adds the current devices */
void UPower::resetDevices() {
  // Removes all devices
  removeDevices();

  // Adds all devices
  GPtrArray* newDevices = up_client_get_devices2(client);
  for (guint i = 0; i < newDevices->len; i++) {
    UpDevice* device = (UpDevice*)g_ptr_array_index(newDevices, i);
    if (device && G_IS_OBJECT(device)) addDevice(device);
  }

  // Update the widget
  dp.emit();
}

bool UPower::show_tooltip_callback(int, int, bool, const Glib::RefPtr<Gtk::Tooltip>& tooltip) {
  return true;
}

const std::string UPower::getDeviceStatus(UpDeviceState& state) {
  switch (state) {
    case UP_DEVICE_STATE_CHARGING:
    case UP_DEVICE_STATE_PENDING_CHARGE:
      return "charging";
    case UP_DEVICE_STATE_DISCHARGING:
    case UP_DEVICE_STATE_PENDING_DISCHARGE:
      return "discharging";
    case UP_DEVICE_STATE_FULLY_CHARGED:
      return "full";
    case UP_DEVICE_STATE_EMPTY:
      return "empty";
    default:
      return "unknown-status";
  }
}

bool UPower::handleToggle(GdkEventButton* const& event) {
  std::lock_guard<std::mutex> guard(m_Mutex);
  showAltText = !showAltText;
  dp.emit();
  return true;
}

std::string UPower::timeToString(gint64 time) {
  if (time == 0) return "";
  float hours = (float)time / 3600;
  float hours_fixed = static_cast<float>(static_cast<int>(hours * 10)) / 10;
  float minutes = static_cast<float>(static_cast<int>(hours * 60 * 10)) / 10;
  if (hours_fixed >= 1) {
    return fmt::format("{H} h", fmt::arg("H", hours_fixed));
  } else {
    return fmt::format("{M} min", fmt::arg("M", minutes));
  }
}

auto UPower::update() -> void {
  std::lock_guard<std::mutex> guard(m_Mutex);

  // Don't update widget if the UPower service isn't running
  if (!upowerRunning) return;

  UpDeviceKind kind;
  UpDeviceState state;
  double percentage;
  gint64 time_empty;
  gint64 time_full;
  gchar* icon_name;

  g_object_get(displayDevice, "kind", &kind, "state", &state, "percentage", &percentage,
               "icon-name", &icon_name, "time-to-empty", &time_empty, "time-to-full", &time_full,
               NULL);

  bool displayDeviceValid =
      kind == UpDeviceKind::UP_DEVICE_KIND_BATTERY || kind == UpDeviceKind::UP_DEVICE_KIND_UPS;

  // CSS status class
  const std::string status = getDeviceStatus(state);
  // Remove last status if it exists
  if (!lastStatus.empty() && box_.get_style_context()->has_class(lastStatus)) {
    box_.get_style_context()->remove_class(lastStatus);
  }
  // Add the new status class to the Box
  if (!box_.get_style_context()->has_class(status)) {
    box_.get_style_context()->add_class(status);
  }
  lastStatus = status;

  if (devices.size() == 0 && !displayDeviceValid && hideIfEmpty) {
    event_box_.set_visible(false);
    // Call parent update
    AModule::update();
    return;
  }

  event_box_.set_visible(true);

  // Tooltip
  if (tooltip_enabled) {
    uint tooltipCount = upower_tooltip->updateTooltip(devices);
    // Disable the tooltip if there aren't any devices in the tooltip
    box_.set_has_tooltip(!devices.empty() && tooltipCount > 0);
  }

  // Set percentage
  std::string percentString = "";
  if (displayDeviceValid) {
    percentString = std::to_string(int(percentage + 0.5)) + "%";
  }

  // Label format
  std::string time_format = "";
  switch (state) {
    case UP_DEVICE_STATE_CHARGING:
    case UP_DEVICE_STATE_PENDING_CHARGE:
      time_format = timeToString(time_full);
      break;
    case UP_DEVICE_STATE_DISCHARGING:
    case UP_DEVICE_STATE_PENDING_DISCHARGE:
      time_format = timeToString(time_empty);
      break;
    default:
      break;
  }
  std::string label_format =
      fmt::format(showAltText ? format_alt : format, fmt::arg("percentage", percentString),
                  fmt::arg("time", time_format));
  // Only set the label text if it doesn't only contain spaces
  bool onlySpaces = true;
  for (auto& character : label_format) {
    if (character == ' ') continue;
    onlySpaces = false;
    break;
  }
  label_.set_markup(onlySpaces ? "" : label_format);

  // Set icon
  if (icon_name == NULL || !Gtk::IconTheme::get_default()->has_icon(icon_name)) {
    icon_name = (char*)"battery-missing-symbolic";
  }
  icon_.set_from_icon_name(icon_name, Gtk::ICON_SIZE_INVALID);

  // Call parent update
  AModule::update();
}

}  // namespace waybar::modules::upower
