#pragma once

#include <libupower-glib/upower.h>

#include <iostream>
#include <map>
#include <string>
#include <unordered_map>

#include "ALabel.hpp"
#include "glibconfig.h"
#include "gtkmm/box.h"
#include "gtkmm/image.h"
#include "gtkmm/label.h"
#include "modules/upower/upower_tooltip.hpp"

namespace waybar::modules::upower {

class UPower : public AModule {
 public:
  UPower(const std::string &, const Json::Value &);
  virtual ~UPower();
  auto update() -> void override;

 private:
  typedef std::unordered_map<std::string, UpDevice *> Devices;

  const std::string DEFAULT_FORMAT = "{percentage}";
  const std::string DEFAULT_FORMAT_ALT = "{percentage} {time}";

  static void deviceAdded_cb(UpClient *client, UpDevice *device, gpointer data);
  static void deviceRemoved_cb(UpClient *client, const gchar *objectPath, gpointer data);
  static void deviceNotify_cb(UpDevice *device, GParamSpec *pspec, gpointer user_data);
  static void prepareForSleep_cb(GDBusConnection *system_bus, const gchar *sender_name,
                                 const gchar *object_path, const gchar *interface_name,
                                 const gchar *signal_name, GVariant *parameters,
                                 gpointer user_data);
  static void upowerAppear(GDBusConnection *conn, const gchar *name, const gchar *name_owner,
                           gpointer data);
  static void upowerDisappear(GDBusConnection *connection, const gchar *name, gpointer user_data);

  void removeDevice(const gchar *objectPath);
  void addDevice(UpDevice *device);
  void setDisplayDevice();
  void resetDevices();
  void removeDevices();
  bool show_tooltip_callback(int, int, bool, const Glib::RefPtr<Gtk::Tooltip> &tooltip);
  bool handleToggle(GdkEventButton *const &) override;
  std::string timeToString(gint64 time);

  const std::string getDeviceStatus(UpDeviceState &state);

  Gtk::Box box_;
  Gtk::Image icon_;
  Gtk::Label label_;

  // Config
  bool hideIfEmpty = true;
  bool tooltip_enabled = true;
  uint tooltip_spacing = 4;
  uint tooltip_padding = 4;
  uint iconSize = 20;
  std::string format = DEFAULT_FORMAT;
  std::string format_alt = DEFAULT_FORMAT_ALT;

  Devices devices;
  std::mutex m_Mutex;
  UpClient *client;
  UpDevice *displayDevice;
  guint login1_id;
  GDBusConnection *login1_connection;
  UPowerTooltip *upower_tooltip;
  std::string lastStatus;
  bool showAltText;
  bool upowerRunning;
  guint upowerWatcher_id;
  std::string nativePath_;
};

}  // namespace waybar::modules::upower
