#pragma once

#include <giomm/dbusconnection.h>
#include <gtkmm/icontheme.h>
#include <libupower-glib/upower.h>

#include <unordered_map>

#include "AIconLabel.hpp"

namespace waybar::modules {

class UPower final : public AIconLabel {
 public:
  UPower(const std::string &, const Json::Value &);
  virtual ~UPower();
  auto update() -> void override;

 private:
  const std::string NO_BATTERY{"battery-missing-symbolic"};

  // Config
  bool showIcon_{true};
  bool hideIfEmpty_{true};
  int iconSize_{20};
  int tooltip_spacing_{4};
  int tooltip_padding_{4};
  Gtk::Box contentBox_;  // tooltip box
  std::string tooltipFormat_;

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

  // Technical variables
  std::string nativePath_;
  std::string model_;
  std::string lastStatus_;
  Glib::ustring label_markup_;
  std::mutex mutex_;
  Glib::RefPtr<Gtk::IconTheme> gtkTheme_;
  bool sleeping_;

  // Technical functions
  void addDevice(UpDevice *);
  void removeDevice(const gchar *);
  void removeDevices();
  void resetDevices();
  void setDisplayDevice();
  const Glib::ustring getText(const upDevice_output &upDevice_, const std::string &format);
  bool queryTooltipCb(int, int, bool, const Glib::RefPtr<Gtk::Tooltip> &);

  // DBUS variables
  guint watcherID_;
  Glib::RefPtr<Gio::DBus::Connection> conn_;
  guint subscrID_{0u};

  // UPower variables
  UpClient *upClient_;
  upDevice_output upDevice_;  // Device to display
  typedef std::unordered_map<std::string, upDevice_output> Devices;
  Devices devices_;
  bool upRunning_{true};

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
  // UPower secondary functions
  void getUpDeviceInfo(upDevice_output &upDevice_);
};

}  // namespace waybar::modules
