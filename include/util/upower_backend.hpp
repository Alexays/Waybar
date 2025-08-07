#pragma once

#include <libupower-glib/upower.h>
#include <spdlog/spdlog.h>

#include <mutex>
#include <string>

#include "util/backend_common.hpp"

namespace waybar::util {

// UPower device info
struct upDevice_output {
  UpDevice *upDevice{NULL};
  double percentage{0.0};
  double temperature{0.0};
  guint64 time_full{0u};
  guint64 time_empty{0u};
  gchar *icon_name{(char *)'\0'};
  bool upDeviceValid{false};
  UpDeviceState state;
  UpDeviceKind kind;
  char *nativePath{(char *)'\0'};
  char *model{(char *)'\0'};
};

class UPowerBackend {
 public:
  explicit UPowerBackend(std::function<void(bool)> notify_cb);
  ~UPowerBackend();

  void getUpDeviceInfo(upDevice_output &upDevice_);

  std::unordered_map<std::string, upDevice_output> &devices() { return devices_; }
  bool running() { return upRunning_; }
  UpClient *client() { return upClient_; }

 private:
  void addDevice(UpDevice *);
  void removeDevice(const gchar *);
  void notifyDevice(UpDevice *);
  void removeDevices();
  void resetDevices();
  void notify(bool changed) { notify_cb_(changed); }

  // DBus callbacks
  void getConn_cb(Glib::RefPtr<Gio::AsyncResult> &result);
  void onAppear(const Glib::RefPtr<Gio::DBus::Connection> &, const Glib::ustring &,
                const Glib::ustring &);
  void onVanished(const Glib::RefPtr<Gio::DBus::Connection> &, const Glib::ustring &);
  void prepareForSleep_cb(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                          const Glib::ustring &sender_name, const Glib::ustring &object_path,
                          const Glib::ustring &interface_name, const Glib::ustring &signal_name,
                          const Glib::VariantContainerBase &parameters);

  // UPower callbacks
  static void deviceAdded_cb(UpClient *client, UpDevice *device, gpointer data);
  static void deviceRemoved_cb(UpClient *client, const gchar *objectPath, gpointer data);
  static void deviceNotify_cb(UpDevice *device, GParamSpec *pspec, gpointer user_data);

  std::mutex mutex_;
  std::function<void(bool)> notify_cb_;

  // DBUS variables
  guint watcherID_;
  Glib::RefPtr<Gio::DBus::Connection> conn_;
  guint subscrID_{0u};

  // UPower variables
  UpClient *upClient_;
  typedef std::unordered_map<std::string, upDevice_output> Devices;
  Devices devices_;
  bool upRunning_{true};

  bool sleeping_;
};

}  // namespace waybar::util
