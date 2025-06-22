#include "modules/bluetooth.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <sstream>

#include "util/scope_guard.hpp"

namespace {

using GDBusManager = std::unique_ptr<GDBusObjectManager, void (*)(GDBusObjectManager*)>;

auto generateManager() -> GDBusManager {
  GError* error = nullptr;
  waybar::util::ScopeGuard error_deleter([error]() {
    if (error) {
      g_error_free(error);
    }
  });
  GDBusObjectManager* manager = g_dbus_object_manager_client_new_for_bus_sync(
      G_BUS_TYPE_SYSTEM,
      GDBusObjectManagerClientFlags::G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
      "org.bluez", "/", NULL, NULL, NULL, NULL, &error);

  if (error) {
    spdlog::error("g_dbus_object_manager_client_new_for_bus_sync() failed: {}", error->message);
  }

  auto destructor = [](GDBusObjectManager* manager) {
    if (manager) {
      g_object_unref(manager);
    }
  };

  return GDBusManager{manager, destructor};
}

auto getBoolProperty(GDBusProxy* proxy, const char* property_name) -> bool {
  auto gvar = g_dbus_proxy_get_cached_property(proxy, property_name);
  if (gvar) {
    bool property_value = g_variant_get_boolean(gvar);
    g_variant_unref(gvar);
    return property_value;
  }

  spdlog::error("getBoolProperty() failed: doesn't have property {}", property_name);
  return false;
}

auto getOptionalStringProperty(GDBusProxy* proxy, const char* property_name)
    -> std::optional<std::string> {
  auto gvar = g_dbus_proxy_get_cached_property(proxy, property_name);
  if (gvar) {
    std::string property_value = g_variant_get_string(gvar, NULL);
    g_variant_unref(gvar);
    return property_value;
  }

  return std::nullopt;
}

auto getStringProperty(GDBusProxy* proxy, const char* property_name) -> std::string {
  auto property_value = getOptionalStringProperty(proxy, property_name);
  if (!property_value.has_value()) {
    spdlog::error("getStringProperty() failed: doesn't have property {}", property_name);
  }
  return property_value.value_or("");
}

auto getUcharProperty(GDBusProxy* proxy, const char* property_name) -> unsigned char {
  auto gvar = g_dbus_proxy_get_cached_property(proxy, property_name);
  if (gvar) {
    unsigned char property_value;
    g_variant_get(gvar, "y", &property_value);
    g_variant_unref(gvar);

    return property_value;
  }

  spdlog::error("getUcharProperty() failed: doesn't have property {}", property_name);
  return 0;
}

}  // namespace

waybar::modules::Bluetooth::Bluetooth(const std::string& id, const Json::Value& config)
    : ALabel(config, "bluetooth", id, " {status}", 10),
#ifdef WANT_RFKILL
      rfkill_{RFKILL_TYPE_BLUETOOTH},
#endif
      manager_(generateManager()) {

  if (config_["format-device-preference"].isArray()) {
    std::transform(config_["format-device-preference"].begin(),
                   config_["format-device-preference"].end(),
                   std::back_inserter(device_preference_), [](auto x) { return x.asString(); });
  }

  if (cur_controller_ = findCurController(); !cur_controller_) {
    if (config_["controller-alias"].isString()) {
      spdlog::warn("no bluetooth controller found with alias '{}'",
                   config_["controller-alias"].asString());
    } else {
      spdlog::warn("no bluetooth controller found");
    }
    update();
  } else {
    // This call only make sense if a controller could be found
    findConnectedDevices(cur_controller_->path, connected_devices_);
  }

  g_signal_connect(manager_.get(), "object-added", G_CALLBACK(onObjectAdded), this);
  g_signal_connect(manager_.get(), "object-removed", G_CALLBACK(onObjectRemoved), this);
  g_signal_connect(manager_.get(), "interface-proxy-properties-changed",
                   G_CALLBACK(onInterfaceProxyPropertiesChanged), this);
  g_signal_connect(manager_.get(), "interface-added", G_CALLBACK(onInterfaceAddedOrRemoved), this);
  g_signal_connect(manager_.get(), "interface-removed", G_CALLBACK(onInterfaceAddedOrRemoved),
                   this);

#ifdef WANT_RFKILL
  rfkill_.on_update.connect(sigc::hide(sigc::mem_fun(*this, &Bluetooth::update)));
#endif

  dp.emit();
}

auto waybar::modules::Bluetooth::update() -> void {
  // focussed device is either:
  // - the first device in the device_preference_ list that is connected to the
  //   current controller (if none fallback to last connected device)
  // - it is the last device that connected to the current controller
  if (!connected_devices_.empty()) {
    bool preferred_device_connected = false;
    if (!device_preference_.empty()) {
      for (const std::string& device_alias : device_preference_) {
        auto it =
            std::find_if(connected_devices_.begin(), connected_devices_.end(),
                         [device_alias](auto device) { return device_alias == device.alias; });
        if (it != connected_devices_.end()) {
          preferred_device_connected = true;
          cur_focussed_device_ = *it;
          break;
        }
      }
    }
    if (!preferred_device_connected) {
      cur_focussed_device_ = connected_devices_.back();
    }
  }

  std::string state;
  std::string tooltip_format;
  if (cur_controller_) {
    if (!cur_controller_->powered)
      state = "off";
    else if (!connected_devices_.empty())
      state = "connected";
    else
      state = "on";
  } else {
    state = "no-controller";
  }
#ifdef WANT_RFKILL
  if (rfkill_.getState()) state = "disabled";
#endif
  bool battery_available =
      state == "connected" && cur_focussed_device_.battery_percentage.has_value();

#ifdef WANT_RFKILL
  // also adds enabled icon if icon for state is not defined
  std::vector<std::string> states = {state, rfkill_.getState() ? "disabled" : "enabled"};
  std::string icon = getIcon(0, states);
#else
  std::string icon = getIcon(0, state);
#endif
  std::string icon_label = icon;
  std::string icon_tooltip = icon;

  if (!alt_) {
    if (battery_available && config_["format-connected-battery"].isString()) {
      format_ = config_["format-connected-battery"].asString();
      icon_label = getIcon(cur_focussed_device_.battery_percentage.value_or(0));
    } else if (config_["format-" + state].isString()) {
      format_ = config_["format-" + state].asString();
    } else if (config_["format"].isString()) {
      format_ = config_["format"].asString();
    } else {
      format_ = default_format_;
    }
  }
  if (battery_available && config_["tooltip-format-connected-battery"].isString()) {
    tooltip_format = config_["tooltip-format-connected-battery"].asString();
    icon_tooltip = getIcon(cur_focussed_device_.battery_percentage.value_or(0));
  } else if (config_["tooltip-format-" + state].isString()) {
    tooltip_format = config_["tooltip-format-" + state].asString();
  } else if (config_["tooltip-format"].isString()) {
    tooltip_format = config_["tooltip-format"].asString();
  }

  auto update_style_context = [this](const std::string& style_class, bool in_next_state) {
    if (in_next_state && !label_.get_style_context()->has_class(style_class)) {
      label_.get_style_context()->add_class(style_class);
    } else if (!in_next_state && label_.get_style_context()->has_class(style_class)) {
      label_.get_style_context()->remove_class(style_class);
    }
  };
  update_style_context("discoverable", cur_controller_ ? cur_controller_->discoverable : false);
  update_style_context("discovering", cur_controller_ ? cur_controller_->discovering : false);
  update_style_context("pairable", cur_controller_ ? cur_controller_->pairable : false);
  if (!state_.empty()) {
    update_style_context(state_, false);
  }
  update_style_context(state, true);
  state_ = state;

  if (format_.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
    label_.set_markup(fmt::format(
        fmt::runtime(format_), fmt::arg("status", state_),
        fmt::arg("num_connections", connected_devices_.size()),
        fmt::arg("controller_address", cur_controller_ ? cur_controller_->address : "null"),
        fmt::arg("controller_address_type",
                 cur_controller_ ? cur_controller_->address_type : "null"),
        fmt::arg("controller_alias", cur_controller_ ? cur_controller_->alias : "null"),
        fmt::arg("device_address", cur_focussed_device_.address),
        fmt::arg("device_address_type", cur_focussed_device_.address_type),
        fmt::arg("device_alias", cur_focussed_device_.alias), fmt::arg("icon", icon_label),
        fmt::arg("device_battery_percentage",
                 cur_focussed_device_.battery_percentage.value_or(0))));
  }

  if (tooltipEnabled()) {
    bool tooltip_enumerate_connections_ = config_["tooltip-format-enumerate-connected"].isString();
    bool tooltip_enumerate_connections_battery_ =
        config_["tooltip-format-enumerate-connected-battery"].isString();
    if (tooltip_enumerate_connections_ || tooltip_enumerate_connections_battery_) {
      std::stringstream ss;
      for (DeviceInfo dev : connected_devices_) {
        if ((tooltip_enumerate_connections_battery_ && dev.battery_percentage.has_value()) ||
            tooltip_enumerate_connections_) {
          ss << "\n";
          std::string enumerate_format;
          std::string enumerate_icon;
          if (tooltip_enumerate_connections_battery_ && dev.battery_percentage.has_value()) {
            enumerate_format = config_["tooltip-format-enumerate-connected-battery"].asString();
            enumerate_icon = getIcon(dev.battery_percentage.value_or(0));
          } else {
            enumerate_format = config_["tooltip-format-enumerate-connected"].asString();
          }
          ss << fmt::format(
              fmt::runtime(enumerate_format), fmt::arg("device_address", dev.address),
              fmt::arg("device_address_type", dev.address_type),
              fmt::arg("device_alias", dev.alias), fmt::arg("icon", enumerate_icon),
              fmt::arg("device_battery_percentage", dev.battery_percentage.value_or(0)));
        }
      }
      device_enumerate_ = ss.str();
      // don't start the connected devices text with a new line
      if (!device_enumerate_.empty()) {
        device_enumerate_.erase(0, 1);
      }
    }
    label_.set_tooltip_text(fmt::format(
        fmt::runtime(tooltip_format), fmt::arg("status", state_),
        fmt::arg("num_connections", connected_devices_.size()),
        fmt::arg("controller_address", cur_controller_ ? cur_controller_->address : "null"),
        fmt::arg("controller_address_type",
                 cur_controller_ ? cur_controller_->address_type : "null"),
        fmt::arg("controller_alias", cur_controller_ ? cur_controller_->alias : "null"),
        fmt::arg("device_address", cur_focussed_device_.address),
        fmt::arg("device_address_type", cur_focussed_device_.address_type),
        fmt::arg("device_alias", cur_focussed_device_.alias), fmt::arg("icon", icon_tooltip),
        fmt::arg("device_battery_percentage", cur_focussed_device_.battery_percentage.value_or(0)),
        fmt::arg("device_enumerate", device_enumerate_)));
  }

  // Call parent update
  ALabel::update();
}

auto waybar::modules::Bluetooth::onObjectAdded(GDBusObjectManager* manager, GDBusObject* object,
                                               gpointer user_data) -> void {
  ControllerInfo info;
  Bluetooth* bt = static_cast<Bluetooth*>(user_data);

  if (!bt->cur_controller_.has_value() && bt->getControllerProperties(object, info) &&
      (!bt->config_["controller-alias"].isString() ||
       bt->config_["controller-alias"].asString() == info.alias)) {
    bt->cur_controller_ = std::move(info);
    bt->dp.emit();
  }
}

auto waybar::modules::Bluetooth::onObjectRemoved(GDBusObjectManager* manager, GDBusObject* object,
                                                 gpointer user_data) -> void {
  Bluetooth* bt = static_cast<Bluetooth*>(user_data);
  GDBusProxy* proxy_controller;

  if (!bt->cur_controller_.has_value()) {
    return;
  }

  proxy_controller = G_DBUS_PROXY(g_dbus_object_get_interface(object, "org.bluez.Adapter1"));

  if (proxy_controller != NULL) {
    std::string object_path = g_dbus_object_get_object_path(object);

    if (object_path == bt->cur_controller_->path) {
      bt->cur_controller_ = bt->findCurController();
      if (bt->cur_controller_.has_value()) {
        bt->connected_devices_.clear();
        bt->findConnectedDevices(bt->cur_controller_->path, bt->connected_devices_);
      }
      bt->dp.emit();
    }

    g_object_unref(proxy_controller);
  }
}

// NOTE: only for when the org.bluez.Battery1 interface is added/removed after/before a device is
// connected/disconnected
auto waybar::modules::Bluetooth::onInterfaceAddedOrRemoved(GDBusObjectManager* manager,
                                                           GDBusObject* object,
                                                           GDBusInterface* interface,
                                                           gpointer user_data) -> void {
  std::string interface_name = g_dbus_proxy_get_interface_name(G_DBUS_PROXY(interface));
  std::string object_path = g_dbus_proxy_get_object_path(G_DBUS_PROXY(interface));
  if (interface_name == "org.bluez.Battery1") {
    Bluetooth* bt = static_cast<Bluetooth*>(user_data);
    if (bt->cur_controller_.has_value()) {
      auto device = std::find_if(bt->connected_devices_.begin(), bt->connected_devices_.end(),
                                 [object_path](auto d) { return d.path == object_path; });
      if (device != bt->connected_devices_.end()) {
        device->battery_percentage = bt->getDeviceBatteryPercentage(object);
        bt->dp.emit();
      }
    }
  }
}

auto waybar::modules::Bluetooth::onInterfaceProxyPropertiesChanged(
    GDBusObjectManagerClient* manager, GDBusObjectProxy* object_proxy, GDBusProxy* interface_proxy,
    GVariant* changed_properties, const gchar* const* invalidated_properties, gpointer user_data)
    -> void {
  std::string interface_name = g_dbus_proxy_get_interface_name(interface_proxy);
  std::string object_path = g_dbus_object_get_object_path(G_DBUS_OBJECT(object_proxy));

  Bluetooth* bt = static_cast<Bluetooth*>(user_data);

  if (!bt->cur_controller_.has_value()) {
    return;
  }

  if (interface_name == "org.bluez.Adapter1") {
    if (object_path == bt->cur_controller_->path) {
      bt->getControllerProperties(G_DBUS_OBJECT(object_proxy), *bt->cur_controller_);
      bt->dp.emit();
    }
  } else if (interface_name == "org.bluez.Device1" || interface_name == "org.bluez.Battery1") {
    DeviceInfo device;
    bt->getDeviceProperties(G_DBUS_OBJECT(object_proxy), device);
    auto cur_device = std::find_if(bt->connected_devices_.begin(), bt->connected_devices_.end(),
                                   [device](auto d) { return d.path == device.path; });
    if (cur_device == bt->connected_devices_.end()) {
      if (device.connected) {
        bt->connected_devices_.push_back(device);
        bt->dp.emit();
      }
    } else {
      if (!device.connected) {
        bt->connected_devices_.erase(cur_device);
      } else {
        *cur_device = device;
      }
      bt->dp.emit();
    }
  }
}

auto waybar::modules::Bluetooth::getDeviceBatteryPercentage(GDBusObject* object)
    -> std::optional<unsigned char> {
  GDBusProxy* proxy_device_bat =
      G_DBUS_PROXY(g_dbus_object_get_interface(object, "org.bluez.Battery1"));
  if (proxy_device_bat != NULL) {
    unsigned char battery_percentage = getUcharProperty(proxy_device_bat, "Percentage");
    g_object_unref(proxy_device_bat);

    return battery_percentage;
  }
  return std::nullopt;
}

auto waybar::modules::Bluetooth::getDeviceProperties(GDBusObject* object, DeviceInfo& device_info)
    -> bool {
  GDBusProxy* proxy_device = G_DBUS_PROXY(g_dbus_object_get_interface(object, "org.bluez.Device1"));

  if (proxy_device != NULL) {
    device_info.path = g_dbus_object_get_object_path(object);
    device_info.paired_controller = getStringProperty(proxy_device, "Adapter");
    device_info.address = getStringProperty(proxy_device, "Address");
    device_info.address_type = getStringProperty(proxy_device, "AddressType");
    device_info.alias = getStringProperty(proxy_device, "Alias");
    device_info.icon = getOptionalStringProperty(proxy_device, "Icon");
    device_info.paired = getBoolProperty(proxy_device, "Paired");
    device_info.trusted = getBoolProperty(proxy_device, "Trusted");
    device_info.blocked = getBoolProperty(proxy_device, "Blocked");
    device_info.connected = getBoolProperty(proxy_device, "Connected");
    device_info.services_resolved = getBoolProperty(proxy_device, "ServicesResolved");

    g_object_unref(proxy_device);

    device_info.battery_percentage = getDeviceBatteryPercentage(object);

    return true;
  }
  return false;
}

auto waybar::modules::Bluetooth::getControllerProperties(GDBusObject* object,
                                                         ControllerInfo& controller_info) -> bool {
  GDBusProxy* proxy_controller =
      G_DBUS_PROXY(g_dbus_object_get_interface(object, "org.bluez.Adapter1"));

  if (proxy_controller != NULL) {
    controller_info.path = g_dbus_object_get_object_path(object);
    controller_info.address = getStringProperty(proxy_controller, "Address");
    controller_info.address_type = getStringProperty(proxy_controller, "AddressType");
    controller_info.alias = getStringProperty(proxy_controller, "Alias");
    controller_info.powered = getBoolProperty(proxy_controller, "Powered");
    controller_info.discoverable = getBoolProperty(proxy_controller, "Discoverable");
    controller_info.pairable = getBoolProperty(proxy_controller, "Pairable");
    controller_info.discovering = getBoolProperty(proxy_controller, "Discovering");

    g_object_unref(proxy_controller);

    return true;
  }
  return false;
}

auto waybar::modules::Bluetooth::findCurController() -> std::optional<ControllerInfo> {
  std::optional<ControllerInfo> controller_info;

  GList* objects = g_dbus_object_manager_get_objects(manager_.get());
  for (GList* l = objects; l != NULL; l = l->next) {
    GDBusObject* object = G_DBUS_OBJECT(l->data);
    ControllerInfo info;
    if (getControllerProperties(object, info) &&
        (!config_["controller-alias"].isString() ||
         config_["controller-alias"].asString() == info.alias)) {
      controller_info = std::move(info);
      break;
    }
  }
  g_list_free_full(objects, g_object_unref);

  return controller_info;
}

auto waybar::modules::Bluetooth::findConnectedDevices(const std::string& cur_controller_path,
                                                      std::vector<DeviceInfo>& connected_devices)
    -> void {
  GList* objects = g_dbus_object_manager_get_objects(manager_.get());
  for (GList* l = objects; l != NULL; l = l->next) {
    GDBusObject* object = G_DBUS_OBJECT(l->data);
    DeviceInfo device;
    if (getDeviceProperties(object, device) && device.connected &&
        device.paired_controller == cur_controller_->path) {
      connected_devices.push_back(device);
    }
  }
  g_list_free_full(objects, g_object_unref);
}
