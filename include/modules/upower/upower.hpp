#pragma once

#include <libupower-glib/upower.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

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
  static void deviceAdded_cb(UpClient *client, UpDevice *device, gpointer data);
  static void deviceRemoved_cb(UpClient *client, const gchar *object_path, gpointer data);
  static void deviceNotify_cb(gpointer data);
  void        removeDevice(const std::string devicePath);
  void        addDevice(UpDevice *device);
  void        setDisplayDevice();
  void        resetDevices();

  Gtk::Box   box_;
  Gtk::Image icon_;
  Gtk::Label label_;

  // Config
  bool hideIfEmpty = true;
  uint iconSize = 32;

  UpClient                         *client = NULL;
  UpDevice                         *displayDevice = NULL;
  std::map<std::string, UpDevice *> devices;
};

}  // namespace waybar::modules
