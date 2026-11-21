#pragma once

#include <giomm/cancellable.h>
#include <giomm/dbusconnection.h>
#include <gtkmm/icontheme.h>
#include <libupower-glib/upower.h>
#include <sigc++/trackable.h>

#include <unordered_map>

#include "AIconLabel.hpp"

namespace waybar::modules {

// sigc::trackable revokes the pending Connection::get() slot on destruction
class UPower final : public AIconLabel, public sigc::trackable {
 public:
  UPower(const std::string&, const Json::Value&);
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
    UpDevice* upDevice{NULL};
    double percentage{0.0};
    double temperature{0.0};
    guint64 time_full{0u};
    guint64 time_empty{0u};
    std::string icon_name;
    bool upDeviceValid{false};
    UpDeviceState state{UP_DEVICE_STATE_UNKNOWN};
    UpDeviceKind kind{UP_DEVICE_KIND_UNKNOWN};
    std::string nativePath;
    std::string model;
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
  void loadDevice(const std::string&);
  void addDevice(UpDevice*);
  void removeDevice(const gchar*);
  void removeDevices();
  void resetDevices();
  void setDisplayDevice();
  const Glib::ustring getText(const upDevice_output& upDevice_, const std::string& format);
  bool queryTooltipCb(int, int, bool, const Glib::RefPtr<Gtk::Tooltip>&);

  // DBUS variables
  guint watcherID_;
  Glib::RefPtr<Gio::DBus::Connection> conn_;
  guint subscrID_{0u};
  guint subscrAddedID_{0u};
  guint subscrRemovedID_{0u};

  // UPower variables
  upDevice_output upDevice_;  // Device to display
  typedef std::unordered_map<std::string, upDevice_output> Devices;
  Devices devices_;
  // Devices being loaded in the background, by object path
  std::unordered_map<std::string, Glib::RefPtr<Gio::Cancellable>> loading_;
  bool upRunning_{true};

  // DBus callbacks
  void getConn_cb(Glib::RefPtr<Gio::AsyncResult>& result);
  void onAppear(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring&,
                const Glib::ustring&);
  void onVanished(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring&);
  void prepareForSleep_cb(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                          const Glib::ustring& sender_name, const Glib::ustring& object_path,
                          const Glib::ustring& interface_name, const Glib::ustring& signal_name,
                          const Glib::VariantContainerBase& parameters);
  void enumerateDevices_cb(Glib::RefPtr<Gio::AsyncResult>& result);
  void deviceAdded_cb(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                      const Glib::ustring& sender_name, const Glib::ustring& object_path,
                      const Glib::ustring& interface_name, const Glib::ustring& signal_name,
                      const Glib::VariantContainerBase& parameters);
  void deviceRemoved_cb(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                        const Glib::ustring& sender_name, const Glib::ustring& object_path,
                        const Glib::ustring& interface_name, const Glib::ustring& signal_name,
                        const Glib::VariantContainerBase& parameters);

  // UPower callbacks
  static void deviceLoaded_cb(GObject* source, GAsyncResult* result, gpointer data);
  static void deviceNotify_cb(UpDevice* device, GParamSpec* pspec, gpointer user_data);
  // UPower secondary functions
  void getUpDeviceInfo(upDevice_output& upDevice_);
};

}  // namespace waybar::modules
