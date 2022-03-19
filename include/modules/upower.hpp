#pragma once

#include <libupower-glib/upower.h>

#include <iostream>
#include <map>
#include <string>

#include "ALabel.hpp"
#include "gtkmm/box.h"
#include "gtkmm/image.h"
#include "gtkmm/label.h"

namespace waybar::modules {

class UPower : public AModule {
 public:
  UPower(const std::string &, const Json::Value &);
  ~UPower();
  auto update() -> void;

 private:
  typedef std::unordered_map<std::string, UpDevice *> Devices;

  static void deviceAdded_cb(UpClient *client, UpDevice *device, gpointer data);
  static void deviceRemoved_cb(UpClient *client, const gchar *objectPath, gpointer data);
  static void deviceNotify_cb(UpDevice *device, GParamSpec *pspec, gpointer user_data);
  static void prepareForSleep_cb(GDBusConnection *system_bus, const gchar *sender_name,
                                 const gchar *object_path, const gchar *interface_name,
                                 const gchar *signal_name, GVariant *parameters,
                                 gpointer user_data);
  void        removeDevice(const gchar *objectPath);
  void        addDevice(UpDevice *device);
  void        setDisplayDevice();
  void        resetDevices();

  Gtk::Box   box_;
  Gtk::Image icon_;
  Gtk::Label label_;

  // Config
  bool hideIfEmpty = true;
  uint iconSize = 32;

  Devices          devices;
  std::mutex       m_Mutex;
  UpClient        *client;
  UpDevice        *displayDevice;
  guint            login1_id;
  GDBusConnection *login1_connection;
};

}  // namespace waybar::modules
