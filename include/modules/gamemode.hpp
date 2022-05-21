#pragma once

#include <iostream>
#include <map>
#include <string>

#include "ALabel.hpp"
#include "giomm/dbusconnection.h"
#include "giomm/dbusproxy.h"
#include "glibconfig.h"
#include "gtkmm/box.h"
#include "gtkmm/image.h"
#include "gtkmm/label.h"
#include "gtkmm/overlay.h"

namespace waybar::modules {

class Gamemode : public AModule {
 public:
  Gamemode(const std::string &, const Json::Value &);
  ~Gamemode();
  auto update() -> void;

 private:
  const std::string DEFAULT_ICON_NAME = "input-gaming-symbolic";
  const std::string DEFAULT_FORMAT = "{glyph}";
  const std::string DEFAULT_FORMAT_ALT = "{glyph} {count}";
  const std::string DEFAULT_TOOLTIP_FORMAT = "Games running: {count}";
  const std::string DEFAULT_GLYPH = "";

  void appear(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name,
              const Glib::ustring &name_owner);
  void disappear(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name);
  void prepareForSleep_cb(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                          const Glib::ustring &sender_name, const Glib::ustring &object_path,
                          const Glib::ustring &interface_name, const Glib::ustring &signal_name,
                          const Glib::VariantContainerBase &parameters);
  void notify_cb(const Glib::ustring &sender_name, const Glib::ustring &signal_name,
                 const Glib::VariantContainerBase &arguments);

  void getData();
  bool handleToggle(GdkEventButton *const &);

  // Config
  std::string format = DEFAULT_FORMAT;
  std::string format_alt = DEFAULT_FORMAT_ALT;
  std::string tooltip_format = DEFAULT_TOOLTIP_FORMAT;
  std::string glyph = DEFAULT_GLYPH;
  bool tooltip = true;
  bool hideNotRunning = true;
  bool useIcon = true;
  uint iconSize = 20;
  uint iconSpacing = 4;
  std::string iconName = DEFAULT_ICON_NAME;

  Gtk::Box box_;
  Gtk::Image icon_;
  Gtk::Label label_;

  const std::string dbus_name = "com.feralinteractive.GameMode";
  const std::string dbus_obj_path = "/com/feralinteractive/GameMode";
  const std::string dbus_interface = "org.freedesktop.DBus.Properties";
  const std::string dbus_get_interface = "com.feralinteractive.GameMode";

  uint gameCount = 0;

  std::string lastStatus;
  bool showAltText = false;

  guint login1_id;
  Glib::RefPtr<Gio::DBus::Proxy> gamemode_proxy;
  Glib::RefPtr<Gio::DBus::Connection> system_connection;
  bool gamemodeRunning;
  guint gamemodeWatcher_id;
};

}  // namespace waybar::modules
