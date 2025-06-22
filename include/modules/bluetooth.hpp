#pragma once

#include "ALabel.hpp"
#ifdef WANT_RFKILL
#include "util/rfkill.hpp"
#endif
#include <gio/gio.h>

#include <optional>
#include <string>
#include <vector>

namespace waybar::modules {

class Bluetooth : public ALabel {
  struct ControllerInfo {
    std::string path;
    std::string address;
    std::string address_type;
    // std::string name; // just use alias instead
    std::string alias;
    bool powered;
    bool discoverable;
    bool pairable;
    bool discovering;
  };

  // NOTE: there are some properties that not all devices provide
  struct DeviceInfo {
    std::string path;
    std::string paired_controller;
    std::string address;
    std::string address_type;
    // std::optional<std::string> name; // just use alias instead
    std::string alias;
    std::optional<std::string> icon;
    bool paired;
    bool trusted;
    bool blocked;
    bool connected;
    bool services_resolved;
    // NOTE: experimental feature in bluez
    std::optional<unsigned char> battery_percentage;
  };

 public:
  Bluetooth(const std::string&, const Json::Value&);
  virtual ~Bluetooth() = default;
  auto update() -> void override;

 private:
  static auto onObjectAdded(GDBusObjectManager*, GDBusObject*, gpointer) -> void;
  static auto onObjectRemoved(GDBusObjectManager*, GDBusObject*, gpointer) -> void;

  static auto onInterfaceAddedOrRemoved(GDBusObjectManager*, GDBusObject*, GDBusInterface*,
                                        gpointer) -> void;
  static auto onInterfaceProxyPropertiesChanged(GDBusObjectManagerClient*, GDBusObjectProxy*,
                                                GDBusProxy*, GVariant*, const gchar* const*,
                                                gpointer) -> void;

  auto getDeviceBatteryPercentage(GDBusObject*) -> std::optional<unsigned char>;
  auto getDeviceProperties(GDBusObject*, DeviceInfo&) -> bool;
  auto getControllerProperties(GDBusObject*, ControllerInfo&) -> bool;

  // Returns std::nullopt if no controller could be found
  auto findCurController() -> std::optional<ControllerInfo>;
  auto findConnectedDevices(const std::string&, std::vector<DeviceInfo>&) -> void;

#ifdef WANT_RFKILL
  util::Rfkill rfkill_;
#endif
  const std::unique_ptr<GDBusObjectManager, void (*)(GDBusObjectManager*)> manager_;

  std::string state_;
  std::optional<ControllerInfo> cur_controller_;
  std::vector<DeviceInfo> connected_devices_;
  DeviceInfo cur_focussed_device_;
  std::string device_enumerate_;

  std::vector<std::string> device_preference_;
};

}  // namespace waybar::modules
